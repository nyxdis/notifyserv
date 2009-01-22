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
	int sockfd;
	struct sockaddr_in addr;
	struct hostent *he;

	if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
		return -1;
	if((he = gethostbyname(prefs.bind_address)) == NULL) {
		errno = h_errno;
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(prefs.bind_port);
	memcpy((char *)&addr.sin_addr.s_addr,(char *)he->h_addr,he->h_length);

	if(fcntl(sockfd,F_SETFL,fcntl(sockfd,F_GETFL,0) | O_NONBLOCK) < 0)
		return -1;

	if(bind(sockfd,(struct sockaddr *)&addr,sizeof(addr)) < 0)
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
	}
	if(prefs.sock_path == NULL && prefs.bind_address == NULL) {
		notify_log(ERROR,"No Unix domain socket path defined and TCP sockets disabled.");
		return -1;
	}
	return 0;
}
