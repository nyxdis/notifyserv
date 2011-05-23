/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


/* Internal data */
struct {
	char **argv;
} notify_info;

/* shut down the main loop */
void notify_shutdown(void);
