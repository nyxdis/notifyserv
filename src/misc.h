/*
 * IRC notification system
 *
 * Copyright (c) 2008, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


enum loglevel {
	NONE,
	ERROR,
	INFO,
	DEBUG
};

int server_connect(const char *host, int port);
void notify_log(enum loglevel level, const char *format, ...);
