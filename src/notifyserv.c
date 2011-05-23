/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include "config.h"
#include "irc.h"
#include "listen.h"
#include "log.h"
#include "notifyserv.h"
#include "preferences.h"

static int daemonise(void);
static void cleanup(void);

static void ns_sighandler(int sig);
static void ns_open_signal_pipe(void);
static void ns_close_signal_pipe(void);
static gboolean ns_signal_parse(GIOChannel *source, GIOCondition condition,
		gpointer data);
static gint signal_pipe[2] = { -1, -1 };
static guint signal_source = 0;

static GMainLoop *loop;

int main(int argc, char *argv[])
{
	struct sigaction sa;

	notify_info.argv = argv;

	g_log_set_default_handler(notify_log, NULL);

	init_preferences(argc, argv);

	log_init();

	/* Fork when wanted */
	if(prefs.fork) daemonise();

	g_message(PACKAGE_STRING "started");

	/* Fire up listening sockets */
	if(start_listener() < 0) {
		if(errno > 0)
			g_critical("Failed to start listener: %s",
					strerror(errno));
		free(prefs.irc_server);
		free(prefs.bind_address);
		cleanup();
		exit(EXIT_FAILURE);
	}

	/* Connect to IRC, retry until successful */
	while (!irc_connect(NULL)) {
		if(errno > 0)
			g_warning("Failed to connect to IRC server: %s",
					strerror(errno));
		g_message("Sleeping for 60 seconds before "
				"retrying IRC connection");
		sleep(60);
	}

	/* Signal handler */
	ns_open_signal_pipe();
	sa.sa_handler = ns_sighandler;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

	g_main_loop_run(loop);

	cleanup();
}

/* fork() wrapper */
static int daemonise(void)
{
	pid_t pid;

	if((pid = fork()) < 0) {
		fputs("Could not fork process.",stderr);
		return -1;
	} else if(pid) {
		exit(EXIT_SUCCESS);
	}
	return 0;
}

/* shut down the main loop */
void notify_shutdown(void)
{
	g_main_loop_quit(loop);
}

/* Cleanup handler, frees still used data */
void cleanup(void)
{
	free(prefs.irc_ident);
	free(prefs.irc_nick);
	free(prefs.irc_server);
	if(prefs.sock_path) unlink(prefs.sock_path);
	free(prefs.sock_path);
}

/* Signal handler function, called by sigaction for SIGINT, SIGTERM and SIGQUIT
 * This function is also called when SIGHUP is received, but doesn't actually
 * do anything in that case (yet) */
static void ns_sighandler(int sig)
{
	if(sig == SIGHUP) {
		g_message("Received signal %d, ignored.", sig);
		return;
	}
	if (write(signal_pipe[1], &sig, 1) < 0) {
		g_warning("Failed to write to signal pipe, reopening");
		ns_close_signal_pipe();
		ns_open_signal_pipe();
	}
}

static void ns_open_signal_pipe(void)
{
	GIOChannel *channel;

	if (pipe(signal_pipe) < 0) {
		g_warning("Failed to open signal pipe");
		return;
	}

	channel = g_io_channel_unix_new(signal_pipe[0]);
	signal_source = g_io_add_watch(channel, G_IO_IN, ns_signal_parse,
			NULL);
	g_io_channel_unref(channel);
}

static void ns_close_signal_pipe(void)
{
	if (signal_pipe[0] > 0)
		close(signal_pipe[0]);
	if (signal_pipe[1] > 0)
		close(signal_pipe[1]);
	signal_pipe[0] = signal_pipe[1] = -1;
}

static gboolean ns_signal_parse(GIOChannel *source,
		G_GNUC_UNUSED GIOCondition condition,
		G_GNUC_UNUSED gpointer data)
{
	GError *error = NULL;
	gchar *buf;

	if (g_io_channel_read_line(source, &buf, NULL, NULL, &error)
			== G_IO_STATUS_ERROR) {
		g_warning("Failed to read from signal pipe: %s",
				error->message);
		ns_close_signal_pipe();
		ns_open_signal_pipe();
		return FALSE;
	}

	g_message("Received signal %s, exiting.", buf);
	notify_shutdown();
	return TRUE;
}
