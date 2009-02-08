/*
 * IRC notification system
 *
 * Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/un.h>
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
static void sighandler(int sig);
static int daemonise(void);

int main(int argc, char *argv[])
{
	int c, chanc = 0, client;
	socklen_t len;
	fd_set read_flags;
	struct sigaction sa;
	struct sockaddr_in in_cli_addr;
	struct sockaddr_un un_cli_addr;
	char buf[512];

	notify_info.argv = argv;

	/* Set defaults */
	prefs.bind_address = strdup("localhost");
	prefs.bind_port = 8675;
	prefs.fork = true;
	prefs.irc_ident = strdup(PACKAGE);
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
	sigaction(SIGHUP,&sa,NULL);

	while((c = getopt(argc,argv,"c:dfhi:l:n:p:s:u:vV")) != -1)
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
					int pos = strcspn(optarg,":");
					prefs.irc_server = malloc(pos+1);
					strncpy(prefs.irc_server,optarg,pos);
					prefs.irc_server[pos] = '\0';
					if(strtol(&optarg[pos+1],NULL,10) > USHRT_MAX) break;
					prefs.irc_port = strtol(&optarg[pos+1],NULL,10);
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

	if(prefs.fork == true) {
		if((notify_info.log_fp = fopen("notifyserv.log","a")) == NULL)
		{
			perror("Unable to open notifyserv.log for logging");
			exit(EXIT_FAILURE);
		}
	}

	if(chanc == 0 || prefs.irc_server == NULL) print_usage(argv[0],EXIT_FAILURE);

	if(prefs.fork == true) daemonise();

	prefs.irc_chans[chanc] = NULL;

	notify_log(INFO,"%s started",PACKAGE_STRING);

	if(start_listener() < 0) {
		if(errno > 0)
			notify_log(ERROR,"Failed to start listener: %s",strerror(errno));
		free(prefs.irc_server);
		free(prefs.bind_address);
		cleanup();
		exit(EXIT_FAILURE);
	}
	if(irc_connect() < 0) {
		if(errno > 0)
			notify_log(ERROR,"Failed to connect to IRC server: %s",strerror(errno));
		cleanup();
		exit(EXIT_FAILURE);
	}

	for(;;)
	{
		FD_ZERO(&read_flags);
		FD_SET(notify_info.irc_sockfd,&read_flags);
		if(notify_info.listen_tcp_sockfd > 0) FD_SET(notify_info.listen_tcp_sockfd,&read_flags);
		if(notify_info.listen_unix_sockfd > 0) FD_SET(notify_info.listen_unix_sockfd,&read_flags);

		if(notify_info.irc_sockfd > notify_info.listen_tcp_sockfd)
			c = notify_info.irc_sockfd;
		if(notify_info.listen_unix_sockfd > c)
			c = notify_info.listen_unix_sockfd;

		if(select(c+1,&read_flags,NULL,NULL,NULL) < 0)
			continue;

		if(FD_ISSET(notify_info.irc_sockfd,&read_flags)) {
			FD_CLR(notify_info.irc_sockfd,&read_flags);
			memset(buf,0,sizeof buf);
			if(read(notify_info.irc_sockfd,buf,sizeof buf - 1) > 0)
				irc_parse(buf);
			else {
				notify_log(DEBUG,"[IRC] Read error: %s",strerror(errno));
				notify_log(INFO,"Lost IRC connection, reconnecting.");
				if(irc_connect() < 0) {
					if(errno > 0)
						notify_log(ERROR,"Failed to connect to IRC server: %s",strerror(errno));
					cleanup();
					exit(EXIT_FAILURE);
				}
			}
		}

		if(notify_info.listen_unix_sockfd > 0) {
			if(FD_ISSET(notify_info.listen_unix_sockfd,&read_flags)) {
				FD_CLR(notify_info.listen_unix_sockfd,&read_flags);
				len = sizeof un_cli_addr;
				client = accept(notify_info.listen_unix_sockfd,(struct sockaddr *)&un_cli_addr,&len);
				memset(buf,0,sizeof buf);
				if(read(client,buf,sizeof buf - 1) > 0)
					listen_forward(0,buf);
				else
					notify_log(ERROR,"Read failed: %s",strerror(errno));
				close(client);
			}
		}

		if(notify_info.listen_tcp_sockfd > 0) {
			if(FD_ISSET(notify_info.listen_tcp_sockfd,&read_flags)) {
				FD_CLR(notify_info.listen_tcp_sockfd,&read_flags);
				len = sizeof in_cli_addr;
				client = accept(notify_info.listen_tcp_sockfd,(struct sockaddr *)&in_cli_addr,&len);
				memset(buf,0,sizeof buf);
				if(read(client,buf,sizeof buf - 1) > 0)
					listen_forward(1,buf);
				else
					notify_log(ERROR,"Read failed: %s",strerror(errno));
				close(client);
			}
		}
	}
}

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
	if(prefs.fork == true) fclose(notify_info.log_fp);
	if(prefs.sock_path != NULL) unlink(prefs.sock_path);
	free(prefs.sock_path);
}

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

static void print_version(void)
{
	puts(PACKAGE_STRING "\n");
	puts("Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>");
	puts("All rights reserved. Released under the 2-clause BSD license.");
	exit(EXIT_SUCCESS);
}

static void sighandler(int sig)
{
	if(sig == SIGHUP) {
		notify_log(INFO,"Received signal %d, ignored.");
		return;
	}
	notify_log(INFO,"Received signal %d, exiting.",sig);
	cleanup();
	exit(EXIT_SUCCESS);
}
