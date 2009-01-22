/*
 * IRC notification system
 *
 * Copyright (c) 2008-2009, Christoph Mende <angelos@unkreativ.org>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#define IDENT "notify"

/* connect to irc */
int irc_connect(void);
void irc_parse(char *string);
void irc_say(const char *channel, const char *string);
