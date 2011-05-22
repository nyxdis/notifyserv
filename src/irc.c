/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
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

/* Send data to the IRC socket after terminating it with \r\n */
static void irc_write(const char *string)
{
	char tmp[IRC_MAX];
	snprintf(tmp,IRC_MAX,"%s\r\n",string);
	if(write(notify_info.irc_sockfd,tmp,strlen(tmp)) < 0) return;
}

/* Prepend 'PRIVMSG chan :' and send that to irc_write */
void irc_say(const char *channel, const char *string)
{
	char tmp[IRC_MAX];
	snprintf(tmp,IRC_MAX,"PRIVMSG %s :%s",channel,string);
	irc_write(tmp);
}

/* Connect to the IRC server */
int irc_connect(void)
{
	char tmp[IRC_MAX];

	notify_info.irc_sockfd = server_connect(prefs.irc_server,prefs.irc_port);
	if(notify_info.irc_sockfd < 0)
		return -1;
	notify_log(INFO,"Connected to IRC server");

	snprintf(tmp,IRC_MAX,"USER %s ns ns :" PACKAGE_STRING,prefs.irc_ident);
	irc_write(tmp);
	snprintf(tmp,IRC_MAX,"NICK %s",prefs.irc_nick);
	irc_write(tmp);

	return 0;
}

/* Parse IRC input */
void irc_parse(const char *line)
{
	char *channel, tmp[IRC_MAX];
	char *nick, *ident, *host;
	int i;

	if(strncmp(line,"ERROR :",7) == 0) {
		if(strstr(line,"Connection timed out")) {
			notify_info.irc_connected = 0;
			close(notify_info.irc_sockfd);
			return;
		}

		notify_log(ERROR,"[IRC] Received error: %s",&line[7]);
		cleanup();
		exit(EXIT_FAILURE);
	}

	snprintf(tmp,IRC_MAX,"433 * %s :Nickname is already in use.\r",prefs.irc_nick);
	if(strstr(line,tmp)) {
		notify_log(ERROR,"[IRC] Nickname is already in use.");
		cleanup();
		exit(EXIT_FAILURE);
	}

	if(strncmp(line,"PING :",6) == 0)
	{
		notify_log(DEBUG,"[IRC] Sending PONG :%s",&line[6]);
		snprintf(tmp,IRC_MAX,"PONG :%s",&line[6]);
		irc_write(tmp);
	}

	snprintf(tmp,IRC_MAX,"001 %s :Welcome to the",prefs.irc_nick);
	if(strstr(line,tmp))
	{
		notify_log(INFO,"[IRC] Connection complete.");
		for(i=0;prefs.irc_chans[i];i++) {
			notify_log(INFO,"[IRC] Joining %s.",prefs.irc_chans[i]);
			snprintf(tmp,IRC_MAX,"JOIN %s",prefs.irc_chans[i]);
			irc_write(tmp);
		}
	}

	if(strstr(line,"#")) {
		if((i = strcspn(strstr(line,"#")," ")) < 2)
			return;
		if(!(channel = malloc(i+1))) return;
		strncpy(channel,strstr(line,"#"),i);
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
		snprintf(tmp,IRC_MAX,"KICK %s %s :",channel,prefs.irc_nick);
		if(strstr(line,tmp)) {
			notify_log(INFO,"[IRC] Kicked from %s, rejoining.",channel);
			snprintf(tmp,IRC_MAX,"JOIN %s",channel);
			irc_write(tmp);
		}

		snprintf(tmp,IRC_MAX,"PRIVMSG %s :!ping\r",channel);
		if(strstr(line,tmp)) {
			notify_log(DEBUG,"[IRC] %s pinged me, sending pong.",nick);
			snprintf(tmp,IRC_MAX,"%s: pong",nick);
			irc_say(channel,tmp);
		}

		snprintf(tmp,IRC_MAX,"PRIVMSG %s :!version\r",channel);
		if(strstr(line,tmp)) {
			notify_log(DEBUG,"[IRC] %s asked for my version.",nick);
			irc_say(channel,"This is " PACKAGE_STRING);
		}

		snprintf(tmp,IRC_MAX,"PRIVMSG %s :!die\r",channel);
		if(strstr(line,tmp)) {
			notify_log(INFO,"Dying as requested by %s (%s@%s) on IRC.",nick,ident,host);
			irc_write("QUIT :Dying");
			free(channel);
			cleanup();
			exit(EXIT_SUCCESS);
		}

		snprintf(tmp,IRC_MAX,"PRIVMSG %s :!reboot\r",channel);
		if(strstr(line,tmp)) {
			free(channel);
			notify_log(INFO,"Rebooting as requested by %s (%s@%s) on IRC.",nick,ident,host);
			cleanup();
			execv(notify_info.argv[0],notify_info.argv);
		}
		free(channel);
	}
}
