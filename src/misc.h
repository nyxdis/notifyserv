/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


typedef enum {
	NONE,
	ERROR,
	INFO,
	DEBUG
} loglevel;

/* generic function to connect to IPv4 servers */
int server_connect(const char *host, unsigned short port);

/* Log events to a logfile or stdout */
void notify_log(loglevel level, const char *format, ...);
