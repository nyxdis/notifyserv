/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include "config.h"
#include "irc.h"
#include "notifyserv.h"
#include "preferences.h"

#define IRC_MAX 512

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
void irc_say(const gchar *channel, const gchar *fmt, ...)
{
	if (irc.ostream) {
		va_list ap;
		gchar *tmp;

		va_start(ap, fmt);
		tmp = g_strdup_vprintf(fmt, ap);
		va_end(ap);

		irc_write("PRIVMSG %s :%s", channel, tmp);
	} else {
		g_warning("Cannot write to IRC: not connected");
	}
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
	gchar *tmp;

	if (strncmp(line, "ERROR :", 7) == 0) {
		if (strstr(line, "Connection timed out")) {
			irc_schedule_reconnect();
			return;
		}

		g_warning("[IRC] Received error: %s", &line[7]);
		notify_shutdown();
		return;
	}

	tmp = g_strdup_printf("433 * %s :Nickname is already in use.",
			prefs.irc_nick);
	if (strstr(line, tmp)) {
		g_critical("[IRC] Nickname is already in use.");
		g_free(tmp);
		notify_shutdown();
		return;
	}
	g_free(tmp);

	if (strncmp(line, "PING :", 6) == 0)
	{
		g_debug("[IRC] Sending PONG :%s", &line[6]);
		irc_write("PONG :%s", &line[6]);
	}

	tmp = g_strdup_printf("001 %s :", prefs.irc_nick);
	if (strstr(line, tmp))
	{
		g_free(tmp);
		g_message("[IRC] Connection complete.");
		for (guint i = 0; prefs.irc_chans[i]; i++) {
			g_message("[IRC] Joining %s.", prefs.irc_chans[i]);
			irc_write("JOIN %s", prefs.irc_chans[i]);
		}
	}
	g_free(tmp);

	if (strstr(line, "PRIVMSG #")) {
		gchar **info;
		gchar *nick, *ident, *host, *channel, *command;

		info = g_strsplit_set(line, ":!@", 9);
		nick = g_strdup(info[1]);
		ident = g_strdup(info[2]);
		host = g_strdup(info[3]);
		channel = g_strdup(info[5]);
		command = g_strdup(info[8]);
		g_strfreev(info);

		/* These rely on channel being initialized */
		if (g_ascii_strcasecmp(command, "ping") == 0) {
			g_debug("[IRC] %s pinged me, sending pong.", nick);
			irc_say(channel, "%s: pong", nick);
		}

		if (g_ascii_strcasecmp(command, "version") == 0) {
			g_debug("[IRC] %s asked for my version.", nick);
			irc_say(channel, "This is " PACKAGE_STRING);
		}

		if (g_ascii_strcasecmp(command, "die") == 0) {
			g_message("Dying as requested by %s (%s@%s) on IRC.",
					nick, ident, host);
			irc_write("QUIT :Dying");
			g_free(channel);
			g_free(nick);
			g_free(ident);
			g_free(host);
			notify_shutdown();
			return;
		}

		if (g_ascii_strcasecmp(command, "reboot") == 0) {
			g_message("Rebooting as requested by %s (%s@%s)"
					" on IRC.", nick, ident, host);
			g_free(channel);
			g_free(nick);
			g_free(ident);
			g_free(host);
			notify_shutdown();
			execv(notify_info.argv[0], notify_info.argv);
		}

		g_free(channel);
		g_free(nick);
		g_free(ident);
		g_free(host);
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
