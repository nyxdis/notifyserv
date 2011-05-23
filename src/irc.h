/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


/* Connect to IRC */
gboolean irc_connect(G_GNUC_UNUSED gpointer data);

/* Send text to an IRC channel */
void irc_say(const gchar *channel, const gchar *string);
