/*
 * IRC notification system
 *
 * Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include "config.h"
#include "misc.h"
#include "irc.h"

struct preferences {
	char *irc_server;
	int irc_port;
	char *irc_chans[20];
	int irc_chanc;
	char *irc_nick;
	char *bind_address;
	int bind_port;
	char *sock_path;
	unsigned int verbosity;
} prefs;

struct notify_info {
	int irc_sockfd;
	int listen_sockfd;
	FILE *log_fp;
} notify_info;

/* clean up sockets */
void cleanup();
