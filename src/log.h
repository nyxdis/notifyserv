/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <glib.h>

void	log_init	(void);
void	notify_log	(const gchar   *log_domain,
			 GLogLevelFlags log_level,
			 const gchar   *message,
			 gpointer       user_data);
void	log_cleanup	(void);

#endif /* __LOG_H__ */
