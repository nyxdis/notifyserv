/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include "notifyserv.h"

static void irc_write(const gchar *fmt, ...);
static void irc_connect_cb(GSocketClient *client, GAsyncResult *result,
		gpointer user_data);
static void irc_schedule_reconnect(void);
static void irc_source_attach(void);
static gboolean irc_callback(gpointer user_data);
static void irc_parse(const gchar *line);

static struct {
	GSocketConnection *connection;
	GOutputStream *ostream;
	GInputStream *istream;
	GSource *callback_source;
	guint reconnect_source;
} irc;

/* Send data to the IRC socket after terminating it with \r\n */
static void irc_write(const gchar *fmt, ...)
{
	GError *error = NULL;
	gchar *tmp1, *tmp2;
	va_list ap;

	if (!irc.ostream)
		return;

	va_start(ap, fmt);
	tmp1 = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	tmp2 = g_strconcat(tmp1, "\n", NULL);
	g_free(tmp1);

	if (g_output_stream_write(irc.ostream, tmp2, strlen(tmp2),
				NULL, &error) < 0) {
		g_warning("Failed to write: %s", error->message);
		g_error_free(error);
	}

	g_free(tmp2);
}

/* Prepend 'PRIVMSG chan :' and send that to irc_write */
void irc_say(const gchar *channel, const gchar *string)
{
	irc_write("PRIVMSG %s :%s", channel, string);
}

/* Connect to the IRC server */
gboolean irc_connect(G_GNUC_UNUSED gpointer data)
{
	GSocketClient *client;

	client = g_socket_client_new();
	g_socket_client_connect_to_host_async(client, prefs.irc_server, 6667,
			NULL, (GAsyncReadyCallback) irc_connect_cb, NULL);
	g_object_unref(client);

	return FALSE;
}

static void irc_connect_cb(GSocketClient *client, GAsyncResult *result,
		G_GNUC_UNUSED gpointer user_data)
{
	GError *error = NULL;

	if (irc.reconnect_source > 0) {
		g_source_remove(irc.reconnect_source);
		irc.reconnect_source = 0;
	}

	irc.connection = g_socket_client_connect_finish(client, result, &error);
	if (irc.connection) {
		g_warning("Failed to connect to IRC: %s", error->message);
		g_error_free(error);
		irc_schedule_reconnect();
		return;
	}

	g_message("Connected to IRC server");

	irc.ostream = g_io_stream_get_output_stream(
			G_IO_STREAM(irc.connection));
	irc.istream = g_io_stream_get_input_stream(
			G_IO_STREAM(irc.connection));

	irc_write("USER %s 0 * :" PACKAGE_STRING, prefs.irc_ident);
	irc_write("NICK %s", prefs.irc_nick);

	irc_source_attach();
}

/* Parse IRC input */
static void irc_parse(const gchar *line)
{
	char *channel, tmp[IRC_MAX];
	char *nick, *ident, *host;
	int i;

	if(strncmp(line,"ERROR :",7) == 0) {
		if(strstr(line,"Connection timed out")) {
			notify_info.irc_connected = 0;
			close(notify_info.irc_sockfd);
			return;
		}

		g_warning("[IRC] Received error: %s", &line[7]);
		cleanup();
		exit(EXIT_FAILURE);
	}

	snprintf(tmp,IRC_MAX,"433 * %s :Nickname is already in use.\r",prefs.irc_nick);
	if(strstr(line,tmp)) {
		g_critical("[IRC] Nickname is already in use.");
		cleanup();
		exit(EXIT_FAILURE);
	}

	if(strncmp(line,"PING :",6) == 0)
	{
		g_debug("[IRC] Sending PONG :%s", &line[6]);
		snprintf(tmp,IRC_MAX,"PONG :%s",&line[6]);
		irc_write(tmp);
	}

	snprintf(tmp,IRC_MAX,"001 %s :Welcome to the",prefs.irc_nick);
	if(strstr(line,tmp))
	{
		g_message("[IRC] Connection complete.");
		for(i=0;prefs.irc_chans[i];i++) {
			g_message("[IRC] Joining %s.", prefs.irc_chans[i]);
			snprintf(tmp,IRC_MAX,"JOIN %s",prefs.irc_chans[i]);
			irc_write(tmp);
		}
	}

	if(strstr(line,"#")) {
		if((i = strcspn(strstr(line,"#")," ")) < 2)
			return;
		if(!(channel = malloc(i+1))) return;
		strncpy(channel,strstr(line,"#"),i);
		channel[i] = '\0';

		/* we only care about the nick here, not before */
		nick = malloc(strcspn(line," ")+1);
		strncpy(nick,&line[1],strcspn(line," "));
		host = strdup(&nick[strcspn(nick,"@")+1]);
		host[strcspn(host," ")] = 0;
		nick[strcspn(nick,"@")] = 0;
		ident = strdup(&nick[strcspn(nick,"!")+1]);
		nick[strcspn(nick,"!")] = 0;

		/* These rely on channel being initialized */
		snprintf(tmp,IRC_MAX,"KICK %s %s :",channel,prefs.irc_nick);
		if(strstr(line,tmp)) {
			g_message("[IRC] Kicked from %s, rejoining.", channel);
			snprintf(tmp,IRC_MAX,"JOIN %s",channel);
			irc_write(tmp);
		}

		snprintf(tmp,IRC_MAX,"PRIVMSG %s :!ping\r",channel);
		if(strstr(line,tmp)) {
			g_debug("[IRC] %s pinged me, sending pong.", nick);
			snprintf(tmp,IRC_MAX,"%s: pong",nick);
			irc_say(channel,tmp);
		}

		snprintf(tmp,IRC_MAX,"PRIVMSG %s :!version\r",channel);
		if(strstr(line,tmp)) {
			g_debug("[IRC] %s asked for my version.", nick);
			irc_say(channel,"This is " PACKAGE_STRING);
		}

		snprintf(tmp,IRC_MAX,"PRIVMSG %s :!die\r",channel);
		if(strstr(line,tmp)) {
			g_message("Dying as requested by %s (%s@%s) on IRC.",
					nick, ident, host);
			irc_write("QUIT :Dying");
			free(channel);
			cleanup();
			exit(EXIT_SUCCESS);
		}

		snprintf(tmp,IRC_MAX,"PRIVMSG %s :!reboot\r",channel);
		if(strstr(line,tmp)) {
			free(channel);
			g_message("Rebooting as requested by %s (%s@%s)"
					" on IRC.", nick, ident, host);
			cleanup();
			execv(notify_info.argv[0],notify_info.argv);
		}
		free(channel);
	}
}

static void irc_schedule_reconnect(void)
{
	irc.reconnect_source = g_timeout_add_seconds(30, irc_connect, NULL);
}

static void irc_source_attach(void)
{
	GSocket *socket = g_socket_connection_get_socket(irc.connection);
	irc.callback_source = g_socket_create_source(socket, G_IO_IN, NULL);
	g_source_set_callback(irc.callback_source, (GSourceFunc) irc_callback,
			NULL, NULL);
	g_source_attach(irc.callback_source, NULL);
}

static gboolean irc_callback(G_GNUC_UNUSED gpointer user_data)
{
	GError *error = NULL;
	gchar *buf, **lines;
	gssize len;

	buf = g_malloc0(IRC_MAX);
	len = g_input_stream_read(irc.istream, buf, IRC_MAX, NULL, &error);
	if (len < 0) {
		g_free(buf);
		g_warning("Failed to read from IRC: %s", error->message);
		g_error_free(error);
		irc_schedule_reconnect();
		return FALSE;
	} else if (len > 0) {
		lines = g_strsplit(buf, "\r\n", 0);
		for (guint i = 0; lines[i] != NULL; i++)
			irc_parse(lines[i]);
		g_strfreev(lines);
	}
	g_free(buf);

	return TRUE;
}
