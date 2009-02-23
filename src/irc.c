/*
 * IRC notification system
 *
 * Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <errno.h>
#include "notifyserv.h"

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

static void irc_write(const char *string)
{
	char *tmp;
	if(asprintf(&tmp,"%s\r\n",string) < 0) return;
	if(write(notify_info.irc_sockfd,tmp,strlen(tmp)) < 0) return;
	free(tmp);
}

void irc_say(const char *channel, const char *string)
{
	char *tmp;
	if(asprintf(&tmp,"PRIVMSG %s :%s",channel,string) < 0) return;
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

	if(asprintf(&tmp,"USER %s ns ns :%s",prefs.irc_ident,PACKAGE_STRING) < 0) return -1;
	irc_write(tmp);
	free(tmp);
	if(asprintf(&tmp,"NICK %s",prefs.irc_nick) < 0) return -1;
	irc_write(tmp);
	free(tmp);

	return 0;
}

void irc_parse(char *line)
{
	char *channel, *tmp;
	char *nick, *ident, *host;
	int i;

	if(strncmp(line,"ERROR :",7) == 0) {
		notify_log(ERROR,"[IRC] Received error: %s",&line[7]);
		cleanup();
		exit(EXIT_FAILURE);
	}

	if(asprintf(&tmp,"433 * %s :Nickname is already in use.\r",prefs.irc_nick) < 0) return;
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
		if(asprintf(&tmp,"PONG :%s",&line[6]) < 0) return;
		irc_write(tmp);
		free(tmp);
	}

	if(asprintf(&tmp,"001 %s :Welcome to the",prefs.irc_nick) < 0) return;
	if(strstr(line,tmp))
	{
		notify_log(INFO,"[IRC] Connection complete.");
		for(i=0;prefs.irc_chans[i] != NULL;i++)
		{
			free(tmp);
			notify_log(INFO,"[IRC] Joining %s.",prefs.irc_chans[i]);
			if(asprintf(&tmp,"JOIN %s",prefs.irc_chans[i]) < 0) return;
			irc_write(tmp);
		}
	}
	free(tmp);

	if((tmp = strstr(line,"#")) != NULL) {
		if((i = strcspn(tmp," ")) < 2)
			return;
		if((channel = malloc(i+1)) == NULL) return;
		memcpy(channel,tmp,i);
		channel[i] = '\0';

		/* we only care about the nick here, not before */
		nick = malloc(strcspn(line," ")+1);
		strncpy(nick,&line[1],strcspn(line," "));
		host = strdup(&nick[strcspn(nick,"@")+1]);
		host[strcspn(host," ")] = 0;
		nick[strcspn(nick,"@")] = 0;
		ident = strdup(&nick[strcspn(nick,"!")+1]);
		nick[strcspn(nick,"!")] = 0;

		/* These rely on channel being initialized */
		if(asprintf(&tmp,"KICK %s %s :",channel,prefs.irc_nick) < 0) return;
		if(strstr(line,tmp))
		{
			notify_log(INFO,"[IRC] Kicked from %s, rejoining.",channel);
			free(tmp);
			if(asprintf(&tmp,"JOIN %s",channel) < 0) return;
			irc_write(tmp);
		}
		free(tmp);

		if(asprintf(&tmp,"PRIVMSG %s :!ping\r",channel) < 0) return;
		if(strstr(line,tmp))
		{
			notify_log(DEBUG,"[IRC] %s pinged me, sending pong.",nick);
			free(tmp);
			if(asprintf(&tmp,"%s: pong",nick) < 0) return;
			irc_say(channel,tmp);
		}
		free(tmp);

		if(asprintf(&tmp,"PRIVMSG %s :!version\r",channel) < 0) return;
		if(strstr(line,tmp))
		{
			notify_log(DEBUG,"[IRC] %s asked for my version.",nick);
			free(tmp);
			if(asprintf(&tmp,"This is %s",PACKAGE_STRING) < 0) return;
			irc_say(channel,tmp);
		}
		free(tmp);

		if(asprintf(&tmp,"PRIVMSG %s :!die\r",channel) < 0) return;
		if(strstr(line,tmp))
		{
			notify_log(INFO,"Dying as requested by %s (%s@%s) on IRC.",nick,ident,host);
			irc_write("QUIT :Dying");
			free(channel);
			free(tmp);
			cleanup();
			exit(EXIT_SUCCESS);
		}
		free(tmp);

		if(asprintf(&tmp,"PRIVMSG %s :!reboot\r",channel) < 0) return;
		if(strstr(line,tmp))
		{
			free(tmp);
			free(channel);
			notify_log(INFO,"Rebooting as requested by %s (%s@%s) on IRC.",nick,ident,host);
			cleanup();
			execv(notify_info.argv[0],notify_info.argv);
		}
		free(tmp);
		free(channel);
	}
}
