/*
 * IRC notification system
 *
 * Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


/* Connect to IRC */
int irc_connect(void);

/* Parse input read on the IRC sockfd */
void irc_parse(char *string);

/* Send text to an IRC channel */
void irc_say(const char *channel, const char *string);
