/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */

#include "config.h"

#include "preferences.h"

#include <glib.h>
#include <stdlib.h>
#include <stdio.h>

static gboolean set_verbosity(const gchar *option_name, const gchar *value,
		gpointer data, GError **error);
static gboolean print_version(const gchar *option_name, const gchar *value,
		gpointer data, GError **error);

void init_preferences(int argc, char *argv[])
{
	GError *error = NULL;
	GOptionContext *context;
	gchar **channels = NULL, *ident = PACKAGE;
	gchar *listen_address = "localhost", *nick = PACKAGE_NAME;
	gchar *irc_server = NULL, *listen_path = NULL;
	gboolean foreground = FALSE;
	gint port = 8675;
	GOptionEntry entries[] = {
		{ "channel", 'c', 0, G_OPTION_ARG_STRING_ARRAY, &channels,
			"Output channel(s), may be given more than once",
			"channel" },
		{ "foreground", 'f', 0, G_OPTION_ARG_NONE, &foreground,
			"Run in foreground", NULL },
		{ "ident", 'i', 0, G_OPTION_ARG_STRING, &ident,
			"IRC ident (optional, " PACKAGE " by default)",
			"ident" },
		{ "listen", 'l', 0, G_OPTION_ARG_STRING, &listen_address,
			"Listen on the specified address (optional, localhost "
				"by default)", "address" },
		{ "nick", 'n', 0, G_OPTION_ARG_STRING, &nick,
			"IRC nick (optional, " PACKAGE_NAME " by default)",
			"nick" },
		{ "port", 'p', 0, G_OPTION_ARG_INT, &port, "Listening port "
			"(optional, 8675 by default)", "port" },
		{ "irc-server", 's', 0, G_OPTION_ARG_STRING, &irc_server,
			"IRC server, default port is 6667", "address[:port]" },
		{ "unix-path", 'u', 0, G_OPTION_ARG_FILENAME, &listen_path,
			"Path to UNIX domain socket", "path" },
		{ "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
			set_verbosity, "Increase logging verbosity, may be "
				"given more than once", NULL },
		{ "version", 'V', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
			print_version, "Print the version", NULL },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};

	/* needs to be set before parsing so the callback works */
	prefs.verbosity = G_LOG_LEVEL_CRITICAL;
	context = g_option_context_new("Connect to IRC and relay every "
			"message received on the listener");
	g_option_context_add_main_entries(context, entries, NULL);
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		g_warning("Parsing command-line options failed: %s",
				error->message);
		g_error_free(error);
	}
	g_option_context_free(context);

	if (!channels)
		g_critical("No IRC channels defined");

	if (!irc_server)
		g_critical("No IRC server defined");

	prefs.irc_chans = g_strdupv(channels);
	prefs.irc_server = g_strdup(irc_server);
	prefs.irc_ident = g_strdup(ident);
	prefs.bind_address = g_strdup(listen_address);
	prefs.irc_nick = g_strdup(nick);
	prefs.sock_path = g_strdup(listen_path);
	prefs.fork = !foreground;
	prefs.bind_port = port;
}

static gboolean set_verbosity(G_GNUC_UNUSED const gchar *option_name,
		G_GNUC_UNUSED const gchar *value,
		G_GNUC_UNUSED gpointer data,
		G_GNUC_UNUSED GError **error)
{
	if (prefs.verbosity == G_LOG_LEVEL_CRITICAL)
		prefs.verbosity = G_LOG_LEVEL_WARNING;
	else if (prefs.verbosity == G_LOG_LEVEL_WARNING)
		prefs.verbosity = G_LOG_LEVEL_MESSAGE;
	else if (prefs.verbosity == G_LOG_LEVEL_MESSAGE)
		prefs.verbosity = G_LOG_LEVEL_DEBUG;

	return TRUE;
}

/* Print the version */
static gboolean print_version(G_GNUC_UNUSED const gchar *option_name,
		G_GNUC_UNUSED const gchar *value,
		G_GNUC_UNUSED gpointer data,
		G_GNUC_UNUSED GError **error)
{
	puts(PACKAGE_STRING "\n");
	puts("Copyright (c) 2008-2011, "
			"Christoph Mende <mende.christoph@gmail.com>");
	puts("All rights reserved. Released under the 2-clause BSD license.");
	exit(EXIT_SUCCESS);
}
