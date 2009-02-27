/*
 * IRC notification system
 *
 * Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <sys/un.h>
#include "notifyserv.h"

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

/* Generic IPv6-capable connect function that opens a socket and connects it */
int server_connect(const char *host, unsigned short port)
{
	char service[6];
	fd_set write_flags;
	int sockfd = 0, valopt, ret;
	socklen_t lon;
	struct addrinfo hints, *result, *rp;
	struct timeval timeout;

	memset(&hints,0,sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	sprintf(service,"%hu",port);

	if((ret = getaddrinfo(host,service,&hints,&result)) != 0) {
		notify_log(ERROR,"[IRC] Failed to get address information: %s",gai_strerror(ret));
		freeaddrinfo(result);
		return -1;
	}

	for(rp = result;rp != NULL;rp = rp->ai_next) {
		if((sockfd = socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol)) >= 0)
			break;
		close(sockfd);
	}

	if(fcntl(sockfd,F_SETFL,fcntl(sockfd,F_GETFL,0) | O_NONBLOCK) < 0) {
		freeaddrinfo(result);
		return -1;
	}

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
					freeaddrinfo(result);
					return -1;
				}
			}
			else {
				errno = ETIMEDOUT;
				freeaddrinfo(result);
				return -1;
			}
		}
		else {
			freeaddrinfo(result);
			return -1;
		}
	}

	errno = 0;
	freeaddrinfo(result);
	return sockfd;
}

/* Logging function */
void notify_log(enum loglevel level, const char *format, ...)
{
	char *ts;
	time_t t;
	va_list ap;
	FILE *fp;

	if(level > prefs.verbosity) return;

	if(!prefs.fork) fp = stdout;
	else fp = notify_info.log_fp;

	t = time(NULL);
	ts = malloc(22);
	strftime(ts,22,"%Y-%m-%d %H:%M:%S  ",localtime(&t));
	fputs(ts,fp);
	free(ts);

	va_start(ap,format);
	vfprintf(fp,format,ap);
	va_end(ap);

	fputs("\n",fp);
	fflush(fp);
}
