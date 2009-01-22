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
	free(prefs.irc_server);
	if(notify_info.irc_sockfd < 0) {
		notify_log(ERROR,"[IRC] Failed to connect: %s",strerror(errno));
		cleanup();
		return -1;
	}
	notify_log(INFO,"Connected to IRC");

	asprintf(&tmp,"USER %s ns ns :%s",IDENT,PACKAGE_STRING);
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
		asprintf(&tmp,"433 * %s :Nickname is already in use.\r\n",prefs.irc_nick);
		if(strstr(line,tmp)) {
			free(tmp);
			notify_log(ERROR,"[IRC] Nickname is already in use.");
			cleanup();
			exit(EXIT_FAILURE);
		}
		free(tmp);

		if(strtok(line,"#") != NULL) {
			tmp = strtok(NULL," ");
			asprintf(&channel,"#%s",tmp);
		}

		if(strncmp(line,"PING :",6))
		{
			notify_log(DEBUG,"[IRC] Sending PONG :%s.",&line[6]);
			asprintf(&tmp,"PONG :%s",&line[6]);
			irc_write(tmp);
			free(tmp);
			continue;
		}

		asprintf(&tmp,"001 %s :Welcome to the",prefs.irc_nick);
		if(strstr(line,tmp))
		{
			free(tmp);
			notify_log(INFO,"[IRC] Connected.");
			for(i=0;prefs.irc_chans[i] != NULL;i++)
			{
				notify_log(INFO,"[IRC] Joining %s.",prefs.irc_chans[i]);
				asprintf(&tmp,"JOIN %s",prefs.irc_chans[i]);
				irc_write(tmp);
				free(tmp);
			}
			continue;
		}
		free(tmp);

		asprintf(&tmp,"KICK %s %s :",channel,prefs.irc_nick);
		if(strstr(line,tmp))
		{
			free(tmp);
			notify_log(INFO,"[IRC] Kicked from %s, rejoining.",channel);
			asprintf(&tmp,"JOIN %s",channel);
			irc_write(tmp);
			free(tmp);
		}
		free(tmp);

		asprintf(&tmp,"PRIVMSG %s :!ping\r\n",channel);
		if(strstr(line,tmp))
		{
			free(tmp);
			notify_log(DEBUG,"[IRC] %s pinged me, sending pong.",strtok(&line[1]," "));
			asprintf(&tmp,"%s: pong",strtok(&line[1],"!"));
			irc_say(channel,tmp);
			free(tmp);
			continue;
		}
		free(tmp);

		asprintf(&tmp,"PRIVMSG %s :!version\r\n",channel);
		if(strstr(line,tmp))
		{
			free(tmp);
			notify_log(DEBUG,"[IRC] %s asked for my version.",strtok(&line[1]," "));
			asprintf(&tmp,"This is %s",PACKAGE_STRING);
			irc_say(channel,tmp);
			free(tmp);
			continue;
		}

		asprintf(&tmp,"PRIVMSG %s :!die\r\n",channel);
		if(strstr(line,tmp))
		{
			free(tmp);
			notify_log(INFO,"Dying as requested by %s on IRC.",strtok(&line[1]," "));
			irc_write("QUIT :Dying");
			cleanup();
			exit(EXIT_SUCCESS);
		}
		free(tmp);

		asprintf(&tmp,"PRIVMSG %s :!reboot\r\n",channel);
		if(strstr(line,tmp))
		{
			free(tmp);
			notify_log(INFO,"Rebooting as requested by %s on IRC.",strtok(&line[1]," "));
			close(notify_info.irc_sockfd);
			close(notify_info.listen_sockfd);
			unlink(prefs.sock_path);
			//execv(argv[0],argv);
		}
	} while((line = strtok_r(string,"\n",&saveptr)) != NULL);

	free(channel);
}
