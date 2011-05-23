/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


struct {
	gboolean fork;
	gchar **irc_chans;
	gchar *bind_address;
	gchar *irc_ident;
	gchar *irc_nick;
	gchar *irc_server;
	gchar *sock_path;
	guint irc_chanc;
	guint16 bind_port;
	guint16 irc_port;
	gushort verbosity;
} prefs;

void init_preferences(int argc, char *argv[]);
