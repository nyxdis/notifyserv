/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include "notifyserv.h"

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

static void print_usage(const char *exec, int retval);
static void print_version(void);
static int daemonise(void);

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
	int c, chanc = 0, client, pos = 0;
	socklen_t len;
	struct pollfd fds[3];
	struct sigaction sa;
	struct sockaddr cli_addr;
	char buf[MAX_BUF+1];

	notify_info.argv = argv;

	g_log_set_default_handler(notify_log, NULL);

	/* Set defaults */
	prefs.bind_address = g_strdup("localhost");
	prefs.bind_port = 8675;
	prefs.fork = true;
	prefs.irc_ident = g_strdup(PACKAGE);
	prefs.irc_nick = g_strdup(PACKAGE_NAME);
	prefs.irc_port = 6667;
	prefs.sock_path = NULL;
	notify_info.listen_unix_sockfd = -1;
	notify_info.listen_tcp_sockfd = -1;

	/* Signal handler */
	ns_open_signal_pipe();
	sa.sa_handler = ns_sighandler;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

	/* Parse command-line options */
	while((c = getopt(argc,argv,"c:dfhi:l:n:p:s:u:vV")) != -1)
		switch(c) {
			char *tmp;
			case 'c':
				if(!(prefs.irc_chans = malloc(strlen(optarg)*2)))
					exit(EXIT_FAILURE);
				tmp = strtok(optarg,",");
				do {
					prefs.irc_chans[chanc] = tmp;
					chanc++;
				} while((tmp = strtok(NULL,",")));
				break;
			case 'd':
				prefs.bind_address = NULL;
				break;
			case 'f':
				prefs.fork = false;
				break;
			case 'h':
				print_usage(argv[0],EXIT_SUCCESS);
				break;
			case 'i':
				free(prefs.irc_ident);
				prefs.irc_ident = strdup(optarg);
				break;
			case 'l':
				free(prefs.bind_address);
				prefs.bind_address = strdup(optarg);
				break;
			case 'p':
				if(strtol(optarg,NULL,10) > USHRT_MAX) break;
				prefs.bind_port = strtol(optarg,NULL,10);
				break;
			case 'n':
				free(prefs.irc_nick);
				prefs.irc_nick = strdup(optarg);
				break;
			case 's':
				if(strstr(optarg,":")) {
					pos = strcspn(optarg,":");
					prefs.irc_server = malloc(pos+1);
					strncpy(prefs.irc_server,optarg,pos);
					prefs.irc_server[pos] = '\0';
					if(strtol(&optarg[pos+1],NULL,10) > USHRT_MAX) break;
					prefs.irc_port = strtol(&optarg[pos+1],NULL,10);
					pos = 0;
				} else {
					prefs.irc_server = strdup(optarg);
				}
				break;
			case 'u':
				prefs.sock_path = strdup(optarg);
				break;
			case 'v':
				prefs.verbosity++;
				break;
			case 'V':
				print_version();
				break;
		}

	/* Open the log file when forking or log to stdout when opening failed */
	if(prefs.fork) {
		if(!(notify_info.log_fp = fopen("notifyserv.log","a"))) {
			perror("Unable to open notifyserv.log for logging");
			exit(EXIT_FAILURE);
		}
	}

	if(!chanc || !prefs.irc_server) print_usage(argv[0],EXIT_FAILURE);

	/* Fork when wanted */
	if(prefs.fork) daemonise();

	prefs.irc_chans[chanc] = NULL;

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
	notify_info.irc_connected = true;

	fds[0].fd = notify_info.irc_sockfd;
	fds[1].fd = notify_info.listen_tcp_sockfd;
	fds[2].fd = notify_info.listen_unix_sockfd;
	fds[0].events = fds[1].events = fds[2].events = POLLIN;

	for(;;)
	{
		poll(fds,3,1000);

		/* Data available on the IRC socket */
		if(fds[0].revents & POLLIN) {
			memset(buf,0,MAX_BUF+1);
			if(read(fds[0].fd,&buf[pos],MAX_BUF-pos) > 0) {
				char line[MAX_BUF];
				pos = strcspn(buf,"\n");

				while(strstr(buf,"\n")) {
					memcpy(line,buf,pos);
					line[pos] = 0;
					irc_parse(line);
					memmove(buf,&buf[pos+1],MAX_BUF-pos);
					pos = strcspn(buf,"\n");
				}
			} else {
				g_message("Lost IRC connection, reconnecting.");
				notify_info.irc_connected = false;
			}
		}
		if(fds[0].revents & POLLHUP) {
			g_message("Lost IRC connection, reconnecting.");
			notify_info.irc_connected = false;
		}

		if(!notify_info.irc_connected && difftime(time(NULL),notify_info.irc_last_conn_try) > 60) {
			if (!irc_connect(NULL)) {
				if(errno > 0)
					g_warning("Failed to connect to"
							" IRC server: %s",
							strerror(errno));
				notify_info.irc_last_conn_try = time(NULL);
			} else
				notify_info.irc_connected = true;
		}

		for(c=1;c<3;c++) {
			/* Data available on the listening sockets */
			if(fds[c].revents & POLLIN) {
				len = sizeof cli_addr;
				client = accept(fds[c].fd,&cli_addr,&len);
				memset(buf,0,MAX_BUF+1);
				if(read(client,buf,MAX_BUF) > 0)
					listen_forward(buf);
				else
					g_warning("Read failed: %s",
							strerror(errno));
				close(client);
			}
		}
	}
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

/* Cleanup handler, frees still used data */
void cleanup(void)
{
	if(notify_info.irc_sockfd > 0) close(notify_info.irc_sockfd);
	if(notify_info.listen_tcp_sockfd > 0)
		close(notify_info.listen_tcp_sockfd);
	if(notify_info.listen_unix_sockfd > 0)
		close(notify_info.listen_unix_sockfd);
	free(prefs.irc_ident);
	free(prefs.irc_nick);
	free(prefs.irc_server);
	if(prefs.fork) fclose(notify_info.log_fp);
	if(prefs.sock_path) unlink(prefs.sock_path);
	free(prefs.sock_path);
}

/* Help output */
static void print_usage(const char *exec, int retval)
{
	printf("Usage: %s -c <channel> -s <address>[:port] [OPTIONS]\n",exec);
	puts("Connect to IRC and relay every message received on the listener\n");
	puts("Options:");
	puts("\t-c <channel>\tOutput channel(s), may be given more than once");
	puts("\t-d\t\tDisable TCP listener");
	puts("\t-f\t\tRun in foreground");
	puts("\t-h\t\tThis help");
	puts("\t-i <ident>\tIRC ident (optional, " PACKAGE " by default)");
	puts("\t-l <address>\tListen on the specified address (optional, localhost by default)");
	puts("\t-n <nick>\tIRC nick (optional, " PACKAGE_NAME " by default)");
	puts("\t-p <port>\tListening port (optional, 8675 by default)");
	puts("\t-s <address>[:port]\tIRC server, default port is 6667");
	puts("\t-u <path>\tPath to UNIX domain socket");
	puts("\t-v\t\tIncrease logging verbosity, may be given more than once");
	puts("\t-V\t\tPrint the version");
	exit(retval);
}

/* Print the version */
static void print_version(void)
{
	puts(PACKAGE_STRING "\n");
	puts("Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>");
	puts("All rights reserved. Released under the 2-clause BSD license.");
	exit(EXIT_SUCCESS);
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
	g_main_loop_quit(loop);
	return TRUE;
}
