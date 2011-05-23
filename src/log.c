/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <stdio.h>

#include <glib.h>

#include "notifyserv.h"

static FILE *log_fp;

/* Logging function */
void notify_log(G_GNUC_UNUSED const gchar *log_domain, GLogLevelFlags log_level,
		const gchar *message, G_GNUC_UNUSED gpointer user_data)
{
	GDateTime *datetime;
	char *ts;
	FILE *fp;

	if (log_level > prefs.verbosity)
		return;

	if (!prefs.fork || !log_fp)
		fp = stdout;
	else
		fp = log_fp;


	datetime = g_date_time_new_now_local();
	ts = g_date_time_format(datetime, "%Y-%m-%d %H:%M:%S  ");
	fputs(ts, fp);
	g_free(ts);

	fputs(message, fp);
	fputs("\n", fp);
	fflush(fp);
}

void log_init(void)
{
	log_fp = fopen("notifyserv.log", "a");
	if (!log_fp)
		g_critical("Unable to open notifyserv.log for logging");
}

void log_cleanup(void)
{
	fclose(log_fp);

}