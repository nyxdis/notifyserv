/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include "config.h"
#include "preferences.h"

static void print_usage(const char *exec, int retval);
static void print_version(void);

void init_preferences(int argc, char *argv[])
{
	int c, chanc = 0, pos = 0;

	/* Set defaults */
	prefs.bind_address = g_strdup("localhost");
	prefs.bind_port = 8675;
	prefs.fork = TRUE;
	prefs.irc_ident = g_strdup(PACKAGE);
	prefs.irc_nick = g_strdup(PACKAGE_NAME);
	prefs.irc_port = 6667;
	prefs.sock_path = NULL;

	/* Parse command-line options */
	while((c = getopt(argc,argv,"c:dfhi:l:n:p:s:u:vV")) != -1)
		switch(c) {
			char *tmp;
			case 'c':
				if(!(prefs.irc_chans = malloc(strlen(optarg)*2)))
					exit(EXIT_FAILURE);
				tmp = strtok(optarg,",");
				do {
					prefs.irc_chans[chanc] = tmp;
					chanc++;
				} while((tmp = strtok(NULL,",")));
				break;
			case 'd':
				prefs.bind_address = NULL;
				break;
			case 'f':
				prefs.fork = FALSE;
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
					pos = strcspn(optarg,":");
					prefs.irc_server = malloc(pos+1);
					strncpy(prefs.irc_server,optarg,pos);
					prefs.irc_server[pos] = '\0';
					if(strtol(&optarg[pos+1],NULL,10) > USHRT_MAX) break;
					prefs.irc_port = strtol(&optarg[pos+1],NULL,10);
					pos = 0;
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

	if(!chanc || !prefs.irc_server) print_usage(argv[0],EXIT_FAILURE);

	prefs.irc_chans[chanc] = NULL;
}

/* Help output */
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

/* Print the version */
static void print_version(void)
{
	puts(PACKAGE_STRING "\n");
	puts("Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>");
	puts("All rights reserved. Released under the 2-clause BSD license.");
	exit(EXIT_SUCCESS);
}
