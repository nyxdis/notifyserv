/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <stdio.h>
#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif

#include "log.h"
#include "irc.h"
#include "listen.h"

#define MAX_BUF 1024 /* socket read buffer */

/* User-defined options */
struct {
	bool fork;
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
	FILE *log_fp;
	char **argv;
} notify_info;

/* clean up sockets */
void cleanup(void);
