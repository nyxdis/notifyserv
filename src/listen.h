/*
 * IRC notification system
 *
 * Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


/* Initialize listeners, TCP and Unix domain sockets */
int start_listener(void);

/* Forward input from listening sockets */
void listen_forward(char *input);
