/*
 * IRC notification system
 *
 * Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "notifyserv.h"

static void print_usage(const char *exec, int retval);
static void print_version(void);
static void sighandler(int sig);
static int daemonise(void);

int main(int argc, char *argv[])
{
	int c, chanc = 0;
	fd_set read_flags;
	struct sigaction sa;

	/* Set defaults */
	prefs.bind_address = strdup("localhost");
	prefs.bind_port = 8675;
	prefs.fork = true;
	prefs.irc_nick = strdup(PACKAGE_NAME);
	prefs.irc_port = 6667;
	prefs.sock_path = NULL;
	notify_info.listen_unix_sockfd = 0;
	notify_info.listen_tcp_sockfd = 0;

	/* Signal handler */
	sa.sa_handler = sighandler;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGINT,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);
	sigaction(SIGQUIT,&sa,NULL);

	while((c = getopt(argc,argv,"c:dfhl:n:p:s:vV")) != -1)
		switch(c)
		{
			case 'c':
				prefs.irc_chans[chanc] = optarg;
				chanc++;
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
			case 'l':
				prefs.bind_address = optarg;
				break;
			case 'p':
				prefs.bind_port = atoi(optarg);
				break;
			case 'n':
				prefs.irc_nick = optarg;
				break;
			case 's':
				if(strstr(optarg,":")) {
					prefs.irc_server = strdup(strtok(optarg,":"));
					prefs.irc_port = atoi(strtok(NULL,":"));
				} else {
					prefs.irc_server = strdup(optarg);
				}
				break;
			case 'v':
				prefs.verbosity++;
				break;
			case 'V':
				print_version();
				break;
		}

	if((notify_info.log_fp = fopen("notifyserv.log","a")) == NULL)
	{
		perror("Unable to open notifyserv.log for logging");
		exit(EXIT_FAILURE);
	}

	if(chanc == 0 || prefs.irc_server == NULL) print_usage(argv[0],EXIT_FAILURE);

	if(prefs.fork == true) daemonise();

	prefs.irc_chans[chanc] = NULL;

	notify_log(INFO,"%s started",PACKAGE_STRING);

	if(start_listener() < 0)
		exit(EXIT_FAILURE);
	if(irc_connect() < 0)
		exit(EXIT_FAILURE);

	if(notify_info.irc_sockfd > notify_info.listen_tcp_sockfd)
		c = notify_info.irc_sockfd;
	if(notify_info.listen_unix_sockfd > c)
		c = notify_info.listen_unix_sockfd;

	while(1)
	{
		FD_ZERO(&read_flags);
		FD_SET(notify_info.irc_sockfd,&read_flags);
		if(notify_info.listen_tcp_sockfd > 0) FD_SET(notify_info.listen_tcp_sockfd,&read_flags);
		if(notify_info.listen_unix_sockfd > 0) FD_SET(notify_info.listen_unix_sockfd,&read_flags);

		if(select(c+1,&read_flags,NULL,NULL,NULL) < 0)
			continue;

		if(FD_ISSET(notify_info.irc_sockfd,&read_flags)) {
			notify_log(DEBUG,"Data on IRC socket");
		}

		if(notify_info.listen_unix_sockfd > 0) {
			if(FD_ISSET(notify_info.listen_unix_sockfd,&read_flags)) {
				notify_log(DEBUG,"Data on Unix domain socket");
			}
		}

		if(notify_info.listen_tcp_sockfd > 0) {
			if(FD_ISSET(notify_info.listen_tcp_sockfd,&read_flags)) {
				notify_log(DEBUG,"Data on TCP socket");
			}
		}
	}
}

static int daemonise(void)
{
	pid_t pid;

	if((pid = fork()) < 0) {
		fprintf(stderr,"Could not fork process.\n");
		return -1;
	} else if(pid) {
		exit(EXIT_SUCCESS);
	}
	return 0;
}

void cleanup(void)
{
	free(prefs.bind_address);
	if(notify_info.irc_sockfd > 0) close(notify_info.irc_sockfd);
	if(notify_info.listen_tcp_sockfd > 0)
		close(notify_info.listen_tcp_sockfd);
	if(notify_info.listen_unix_sockfd > 0)
		close(notify_info.listen_unix_sockfd);
	free(prefs.irc_nick);
	free(prefs.sock_path);
	fclose(notify_info.log_fp);
	unlink(prefs.sock_path);
}

static void print_usage(const char *exec, int retval)
{
	printf("Usage: %s -c <channel> -s <address>[:port] [OPTIONS]\n",exec);
	printf("Connect to IRC and relay every message received on the listener\n\n");
	printf("Options:\n");
	printf("\t-c <channel>\tOutput channel(s), may be given more than once\n");
	printf("\t-d\t\tDisable TCP listener\n");
	printf("\t-f\t\tRun in foreground\n");
	printf("\t-h\t\tThis help\n");
	printf("\t-l <address>\tListen on the specified address (optional, localhost by default)\n");
	printf("\t-n <nick>\tIRC nick (optional, %s by default)\n",PACKAGE_NAME);
	printf("\t-p <port>\tListening port (optional, 8675 by default)\n");
	printf("\t-s <address>[:port]\tIRC server, default port is 6667\n");
	printf("\t-v\t\tIncrease logging verbosity, may be given more than once\n");
	printf("\t-V\t\tPrint the version\n");
	exit(retval);
}

static void print_version(void)
{
	printf("%s\n\n",PACKAGE_STRING);
	printf("Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>\n");
	printf("All rights reserved. Released under the 2-clause BSD license.\n");
	exit(EXIT_SUCCESS);
}

static void sighandler(int sig)
{
	notify_log(INFO,"Received signal %d, exiting.",sig);
	cleanup();
	exit(EXIT_SUCCESS);
}
