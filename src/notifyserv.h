/*
 * IRC notification system
 *
 * Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <stdbool.h>

#include "config.h"
#include "misc.h"
#include "irc.h"
#include "listen.h"

struct preferences {
	bool fork;
	char *bind_address;
	char *irc_chans[20];
	char *irc_ident;
	char *irc_nick;
	char *irc_server;
	char *sock_path;
	int bind_port;
	int irc_chanc;
	int irc_port;
	unsigned short verbosity;
} prefs;

struct notify_info {
	int irc_sockfd;
	int listen_unix_sockfd;
	int listen_tcp_sockfd;
	FILE *log_fp;
	char **argv;
} notify_info;

/* clean up sockets */
void cleanup(void);
