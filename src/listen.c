/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <string.h>

#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>

#include "irc.h"
#include "listen.h"
#include "preferences.h"

#define BUF_SIZE 1024

static gboolean listen_accept(GSocketService *service,
		GSocketConnection *connection, GObject *src_object,
		gpointer user_data);
static void listen_parse(const gchar *line);

static struct {
	GSocketService *service;
} listen;

/* Start specified listening sockets */
gboolean start_listener(void)
{
	listen.service = g_socket_service_new();

	if (prefs.sock_path) {
		GError *error = NULL;
		GSocketAddress *address;

		address = g_unix_socket_address_new(prefs.sock_path);

		if (!g_socket_listener_add_address(
					G_SOCKET_LISTENER(listen.service),
					address, G_SOCKET_TYPE_STREAM,
					G_SOCKET_PROTOCOL_DEFAULT, NULL, NULL,
					&error)) {
			g_warning("Failed to bind to Unix socket %s: %s",
					prefs.sock_path, error->message);
			g_error_free(error);
		} else {
			g_message("Listening on Unix domain socket %s",
					prefs.sock_path);
		}
	}

	if (prefs.bind_address) {
		GError *error = NULL;
		GList *addresses;
		GResolver *resolver;
		GSocketAddress *saddress;

		resolver = g_resolver_get_default();
		addresses = g_resolver_lookup_by_name(resolver,
				prefs.bind_address, NULL, &error);
		if (!addresses) {
			g_warning("Failed to resolve bind address: %s",
					error->message);
			g_error_free(error);
			return FALSE;
		}
		saddress = g_inet_socket_address_new(addresses->data, prefs.bind_port);
		g_resolver_free_addresses(addresses);

		if (!g_socket_listener_add_address(
					G_SOCKET_LISTENER(listen.service),
					saddress, G_SOCKET_TYPE_STREAM,
					G_SOCKET_PROTOCOL_TCP, NULL, NULL,
					&error)) {
			g_warning("Failed to bind to address %s: %s",
					prefs.bind_address, error->message);
			g_error_free(error);
		} else {
			g_message("Listening on %s:%hu", prefs.bind_address,
					prefs.bind_port);
		}
	}

	if (!prefs.sock_path && !prefs.bind_address) {
		g_critical("No Unix domain socket path defined and TCP sockets"
				" disabled.");
		return FALSE;
	}

	g_signal_connect(listen.service, "incoming",
			G_CALLBACK(listen_accept), NULL);
	g_socket_service_start(listen.service);
	return TRUE;
}

/* Parse input from listening sockets */
static gboolean listen_accept(G_GNUC_UNUSED GSocketService *service,
		GSocketConnection *connection,
		G_GNUC_UNUSED GObject *src_object,
		G_GNUC_UNUSED gpointer user_data)
{
	GError *error = NULL;
	GInputStream *istream;
	gchar *buf, **lines;
	gssize len;

	istream = g_io_stream_get_input_stream(G_IO_STREAM(connection));
	buf = g_malloc0(BUF_SIZE);
	len = g_input_stream_read(istream, buf, BUF_SIZE, NULL, &error);

	if (len < 0) {
		g_warning("Failed to read from client: %s", error->message);
		g_error_free(error);
	} else if (len > 0) {
		lines = g_strsplit(buf, "\r\n", 0);
		for (guint i = 0; lines[i] != NULL; i++)
			listen_parse(lines[i]);
		g_strfreev(lines);
	}

	/* close socket */
	return TRUE;
}

static void listen_parse(const gchar *line)
{
	line = g_strchomp(line);

	if (line[0] != '#') {
		if (line[0] != '*')
			g_message("Received deprecated input format, the first"
					" word should be the channel or *");

		for (guint i = 0; prefs.irc_chans[i]; i++)
			irc_say(prefs.irc_chans[i], line);
		g_message("Forwarded data to IRC: %s", line);
	} else {
		gushort i = strcspn(line," ");
		gchar *channel = g_strndup(line, i);
		irc_say(channel, &line[i]);
		g_message("Forwarded data to IRC channel %s: %s", channel,
				&line[i]);
		g_free(channel);
	}
}
