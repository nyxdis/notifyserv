/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */

#ifndef __LISTEN_H__
#define __LISTEN_H__

#include <glib.h>

/* Initialize listeners, TCP and Unix domain sockets */
gboolean	start_listener	(void);

#endif /* __LISTEN_H__ */
