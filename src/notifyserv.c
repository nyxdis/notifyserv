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

int main(int argc, char *argv[])
{
	int c, chanc = 0;
	struct sigaction sa;

	/* Set defaults */
	prefs.bind_address = strdup("localhost");
	prefs.bind_port = 8675;
	prefs.irc_nick = strdup(PACKAGE_NAME);
	prefs.irc_port = 6667;
	prefs.sock_path = strdup("/tmp/notifyserv");

	/* Signal handler */
	sa.sa_handler = sighandler;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGINT,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);
	sigaction(SIGQUIT,&sa,NULL);

	while((c = getopt(argc,argv,"c:hl:n:p:s:vV")) != -1)
		switch(c)
		{
			case 'c':
				prefs.irc_chans[chanc] = optarg;
				chanc++;
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
	prefs.irc_chans[chanc] = NULL;

	notify_log(INFO,"%s started",PACKAGE_STRING);

	/*if(fork() == 0)
	{*/
		if(irc_connect() < 0)
			exit(EXIT_FAILURE);
		while(1)
		{
			sleep(10);
		}
	/*} else
		exit(EXIT_SUCCESS);*/
}

void cleanup(void)
{
	free(prefs.bind_address);
	if(notify_info.irc_sockfd > 0) close(notify_info.irc_sockfd);
	if(notify_info.listen_sockfd > 0) close(notify_info.listen_sockfd);
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
