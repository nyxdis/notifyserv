/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


/* Initialize listeners, TCP and Unix domain sockets */
int start_listener(void);

/* Forward input from listening sockets */
void listen_forward(char *input);
