/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <glib.h>

typedef enum {
	NONE,
	ERROR,
	INFO,
	DEBUG
} loglevel;

/* generic function to connect to IPv4 servers */
int server_connect(const char *host, unsigned short port);

/* Log events to a logfile or stdout */
void notify_log(G_GNUC_UNUSED const gchar *log_domain, GLogLevelFlags log_level,
		const gchar *message, G_GNUC_UNUSED gpointer user_data);
