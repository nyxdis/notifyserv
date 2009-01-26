/*
 * IRC notification system
 *
 * Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include "notifyserv.h"

static int listen_unix(void)
{
	int sockfd;
	struct sockaddr_un addr;

	if((sockfd = socket(AF_UNIX,SOCK_STREAM,0)) < 0)
		return -1;

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path,prefs.sock_path,strlen(prefs.sock_path) + 1);

	if(fcntl(sockfd,F_SETFL,fcntl(sockfd,F_GETFL,0) | O_NONBLOCK) < 0)
		return -1;

	unlink(addr.sun_path);

	if(bind(sockfd,(struct sockaddr *)&addr,strlen(addr.sun_path) + sizeof(addr.sun_family)) < 0)
		return -1;
	if(listen(sockfd,5) < 0)
		return -1;

	notify_log(INFO,"Listening on Unix domain socket %s",prefs.sock_path);
	return sockfd;
}

static int listen_tcp(void)
{
	int sockfd, ret;
	char service[5];
	struct addrinfo hints, *result, *rp;

	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	sprintf(service,"%d",prefs.bind_port);

	if((ret = getaddrinfo(prefs.bind_address,service,&hints,
		&result)) != 0) {
		errno = ret;
		return -1;
	}

	for(rp = result;rp != NULL;rp = rp->ai_next) {
		if((sockfd = socket(rp->ai_family,rp->ai_socktype,
			rp->ai_protocol)) < 0) continue;

		if(bind(sockfd,rp->ai_addr,rp->ai_addrlen) == 0)
			break;

		close(sockfd);
	}
	freeaddrinfo(result);

	if(fcntl(sockfd,F_SETFL,fcntl(sockfd,F_GETFL,0) | O_NONBLOCK) < 0)
		return -1;

	if(listen(sockfd,5) < 0)
		return -1;

	notify_log(INFO,"Listening on %s:%d",prefs.bind_address,prefs.bind_port);

	return sockfd;
}

int start_listener(void)
{
	if(prefs.sock_path != NULL) {
		if((notify_info.listen_unix_sockfd = listen_unix()) < 0)
			return -1;
	}
	if(prefs.bind_address != NULL) {
		if((notify_info.listen_tcp_sockfd = listen_tcp()) < 0)
			return -1;
		free(prefs.bind_address);
	}
	if(prefs.sock_path == NULL && prefs.bind_address == NULL) {
		notify_log(ERROR,"No Unix domain socket path defined and TCP sockets disabled.");
		return -1;
	}
	return 0;
}

void listen_forward(int orig, char *input)
{
	char *line, *saveptr, *origin;
	int i;

	if(orig == 1) origin = strdup("TCP");
	else origin = strdup("Unix domain");

	line = strtok_r(input,"\n",&saveptr);
	do {
		for(i=0;prefs.irc_chans[i] != NULL;i++)
			irc_say(prefs.irc_chans[i],line);
		notify_log(INFO,"Forwareded data from %s socket to IRC: %s",
			origin,line);
	} while((line = strtok_r(NULL,"\n",&saveptr)) != NULL);
	free(origin);
}
