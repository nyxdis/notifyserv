/*
 * IRC notification system
 *
 * Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/un.h>

#include "notifyserv.h"

int server_connect(const char *host, int port)
{
        fd_set write_flags;
        int sockfd, valopt;
        socklen_t lon;
        struct hostent *he;
        struct sockaddr_in addr;
        struct timeval timeout;
        unsigned int len;

        if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
                return -1;

        if(fcntl(sockfd,F_SETFL,fcntl(sockfd,F_GETFL,0) | O_NONBLOCK) < 0)
                return -1;

        he = gethostbyname(host);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        memcpy((char *)&addr.sin_addr.s_addr,(char *)he->h_addr,he->h_length);
        len = sizeof(addr);

        if(connect(sockfd,(struct sockaddr *)&addr,len) < 0) {
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

	if(level > prefs.verbosity) return;

	t = time(NULL);
	ts = malloc(22);
	strftime(ts,22,"%F %T  ",localtime(&t));
	fputs(ts,notify_info.log_fp);
	free(ts);

	va_start(ap,format);
	vfprintf(notify_info.log_fp,format,ap);
	va_end(ap);

	fputs("\n",notify_info.log_fp);
	fflush(notify_info.log_fp);
}
