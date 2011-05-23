/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


/* User-defined options */
struct {
	gboolean fork;
	gchar *bind_address;
	char **irc_chans;
	char *irc_ident;
	char *irc_nick;
	char *irc_server;
	char *sock_path;
	guint16 bind_port;
	int irc_chanc;
	unsigned short irc_port;
	unsigned short verbosity;
} prefs;

/* Internal data */
struct {
	char **argv;
} notify_info;

/* clean up sockets */
void cleanup(void);
