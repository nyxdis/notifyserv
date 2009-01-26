/*
 * IRC notification system
 *
 * Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "notifyserv.h"

int server_connect(const char *host, int port)
{
	char service[5];
	fd_set write_flags;
	int sockfd = 0, valopt, ret;
	socklen_t lon;
	struct addrinfo hints, *result, *rp;
	struct timeval timeout;

	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	sprintf(service,"%d",port);

	if((ret = getaddrinfo(host,service,&hints,&result)) != 0) {
		notify_log(ERROR,"[IRC] Failed to get address information: %s",gai_strerror(ret));
		return -1;
	}

	for(rp = result;rp != NULL;rp = rp->ai_next) {
		if((sockfd = socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol)) >= 0)
			break;
		close(sockfd);
	}

	if(fcntl(sockfd,F_SETFL,fcntl(sockfd,F_GETFL,0) | O_NONBLOCK) < 0)
		return -1;

	if(connect(sockfd,rp->ai_addr,rp->ai_addrlen) < 0) {
		if(errno == EINPROGRESS) {
			timeout.tv_sec = 15;
			timeout.tv_usec = 0;

			FD_ZERO(&write_flags);
			FD_SET(sockfd,&write_flags);
			if(select(sockfd+1,NULL,&write_flags,NULL,&timeout) > 0) {
				lon = sizeof(int);
				getsockopt(sockfd,SOL_SOCKET,SO_ERROR,
					(void*)(&valopt),&lon);
				if(valopt) {
					errno = valopt;
					return -1;
				}
			}
			else {
				errno = ETIMEDOUT;
				return -1;
			}
		}
		else
			return -1;
	}

	errno = 0;
	return sockfd;
}

void notify_log(enum loglevel level, const char *format, ...)
{
	char *ts;
	time_t t;
	va_list ap;
	FILE *fp;

	if(level > prefs.verbosity) return;

	if(prefs.fork == false) fp = stdout;
	else fp = notify_info.log_fp;

	t = time(NULL);
	ts = malloc(22);
	/* strftime(ts,22,"%F %T  ",localtime(&t)); */
	strftime(ts,22,"%Y-%m-%d %H:%M:%S  ",localtime(&t));
	fputs(ts,fp);
	free(ts);

	va_start(ap,format);
	vfprintf(fp,format,ap);
	va_end(ap);

	fputs("\n",fp);
	fflush(fp);
}
