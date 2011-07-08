/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */

#include "config.h"

#include "notifyserv.h"

#include <glib.h>
#include <glib-object.h>

#include <stdlib.h>
#include <unistd.h>

#include "irc.h"
#include "listen.h"
#include "log.h"
#include "preferences.h"

static void daemonize(void);
static void cleanup(void);

static void ns_sighandler(gint sig);
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

	g_type_init();

	notify_info.argv = argv;

	init_preferences(argc, argv);

	/* Fork when wanted */
	if (prefs.fork)
		daemonize();

	log_init();

	g_log_set_default_handler(notify_log, NULL);

	loop = g_main_loop_new(NULL, FALSE);

	g_message(PACKAGE_STRING " started");

	/* Fire up listening sockets */
	if (!start_listener()) {
		cleanup();
		exit(EXIT_FAILURE);
	}

	/* Connect to IRC */
	irc_connect(NULL);

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
static void daemonize(void)
{
	pid_t pid = fork();

	if (pid < 0) {
		g_critical("Could not fork process:");
	} else if (pid) {
		exit(EXIT_SUCCESS);
	}
}

/* shut down the main loop */
void notify_shutdown(void)
{
	g_main_loop_quit(loop);
}

/* Cleanup handler, frees still used data */
void cleanup(void)
{
	g_free(prefs.irc_ident);
	g_free(prefs.irc_nick);
	g_free(prefs.irc_server);
	if (prefs.sock_path)
		unlink(prefs.sock_path);
	g_free(prefs.sock_path);
}

/* Signal handler function, called by sigaction for SIGINT, SIGTERM and SIGQUIT
 * This function is also called when SIGHUP is received, but doesn't actually
 * do anything in that case (yet) */
static void ns_sighandler(gint sig)
{
	if (sig == SIGHUP) {
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
	gint fd = g_io_channel_unix_get_fd(source);
	gchar sig;

	if (read(fd, &sig, 1) < 0) {
		g_warning("Failed to read from signal pipe.");
		ns_close_signal_pipe();
		ns_open_signal_pipe();
		return FALSE;
	}

	g_message("Received signal %hhd, exiting.", sig);
	notify_shutdown();
	return TRUE;
}
