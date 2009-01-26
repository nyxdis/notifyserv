/*
 * IRC notification system
 *
 * Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "notifyserv.h"

static void irc_write(const char *string)
{
	char *tmp;
	asprintf(&tmp,"%s\r\n",string);
	write(notify_info.irc_sockfd,tmp,strlen(tmp));
	free(tmp);
}

void irc_say(const char *channel, const char *string)
{
	char *tmp;
	asprintf(&tmp,"PRIVMSG %s :%s",channel,string);
	irc_write(tmp);
	free(tmp);
}

int irc_connect(void)
{
	char *tmp;

	notify_info.irc_sockfd = server_connect(prefs.irc_server,prefs.irc_port);
	if(notify_info.irc_sockfd < 0)
		return -1;
	notify_log(INFO,"Connected to IRC server");

	asprintf(&tmp,"USER %s ns ns :%s",prefs.irc_ident,PACKAGE_STRING);
	irc_write(tmp);
	free(tmp);
	asprintf(&tmp,"NICK %s",prefs.irc_nick);
	irc_write(tmp);
	free(tmp);

	return 0;
}

void irc_parse(char *string)
{
	char *channel, *line, *saveptr, *tmp;
	int i;

	line = strtok_r(string,"\n",&saveptr);
	do {
		if(strncmp(line,"ERROR :",7) == 0) {
			notify_log(ERROR,"[IRC] Received error: %s",&line[7]);
			cleanup();
			exit(EXIT_FAILURE);
		}

		asprintf(&tmp,"433 * %s :Nickname is already in use.\r",prefs.irc_nick);
		if(strstr(line,tmp)) {
			free(tmp);
			notify_log(ERROR,"[IRC] Nickname is already in use.");
			cleanup();
			exit(EXIT_FAILURE);
		}
		free(tmp);

		if(strncmp(line,"PING :",6) == 0)
		{
			notify_log(DEBUG,"[IRC] Sending PONG :%s",&line[6]);
			asprintf(&tmp,"PONG :%s",&line[6]);
			irc_write(tmp);
			free(tmp);
		}

		asprintf(&tmp,"001 %s :Welcome to the",prefs.irc_nick);
		if(strstr(line,tmp))
		{
			notify_log(INFO,"[IRC] Connection complete.");
			for(i=0;prefs.irc_chans[i] != NULL;i++)
			{
				free(tmp);
				notify_log(INFO,"[IRC] Joining %s.",prefs.irc_chans[i]);
				asprintf(&tmp,"JOIN %s",prefs.irc_chans[i]);
				irc_write(tmp);
			}
		}
		free(tmp);

		if((tmp = strstr(line,"#")) != NULL) {
			if((i = strcspn(tmp," ")) < 2)
				continue; /* invalid string */
			if((channel = malloc(i+1)) == NULL) return;
			memcpy(channel,tmp,i);
			channel[i] = '\0';

			/* These rely on channel being initialized */
			asprintf(&tmp,"KICK %s %s :",channel,prefs.irc_nick);
			if(strstr(line,tmp))
			{
				notify_log(INFO,"[IRC] Kicked from %s, rejoining.",channel);
				free(tmp);
				asprintf(&tmp,"JOIN %s",channel);
				irc_write(tmp);
			}
			free(tmp);

			asprintf(&tmp,"PRIVMSG %s :!ping\r",channel);
			if(strstr(line,tmp))
			{
				notify_log(DEBUG,"[IRC] %s pinged me, sending pong.",strtok(&line[1]," "));
				free(tmp);
				asprintf(&tmp,"%s: pong",strtok(&line[1],"!"));
				irc_say(channel,tmp);
			}
			free(tmp);

			asprintf(&tmp,"PRIVMSG %s :!version\r",channel);
			if(strstr(line,tmp))
			{
				notify_log(DEBUG,"[IRC] %s asked for my version.",strtok(&line[1]," "));
				free(tmp);
				asprintf(&tmp,"This is %s",PACKAGE_STRING);
				irc_say(channel,tmp);
			}
			free(tmp);

			asprintf(&tmp,"PRIVMSG %s :!die\r",channel);
			if(strstr(line,tmp))
			{
				notify_log(INFO,"Dying as requested by %s on IRC.",strtok(&line[1]," "));
				irc_write("QUIT :Dying");
				free(channel);
				free(tmp);
				cleanup();
				exit(EXIT_SUCCESS);
			}
			free(tmp);

			asprintf(&tmp,"PRIVMSG %s :!reboot\r",channel);
			if(strstr(line,tmp))
			{
				free(tmp);
				free(channel);
				notify_log(INFO,"Rebooting as requested by %s on IRC.",strtok(&line[1]," "));
				cleanup();
				execv(notify_info.argv[0],notify_info.argv);
			}
			free(tmp);
			free(channel);
		}
	} while((line = strtok_r(NULL,"\n",&saveptr)) != NULL);
}
