/*
 * IRC notification system
 *
 * Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


enum loglevel {
	NONE,
	ERROR,
	INFO,
	DEBUG
};

/* generic function to connect to IPv4 servers */
int server_connect(const char *host, unsigned short port);

/* Log events to a logfile or stdout */
void notify_log(enum loglevel level, const char *format, ...);
