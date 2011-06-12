bin_PROGRAMS = notifyserv

notifyserv_SOURCES =	src/notifyserv.c src/notifyserv.h \
			src/irc.c src/irc.h \
			src/listen.c src/listen.h \
			src/log.c src/log.h \
			src/preferences.c src/preferences.h

notifyserv_LDADD =	$(glib_LIBS) \
			$(gio_LIBS) \
			$(gio_unix_LIBS)

notifyserv_CFLAGS =	$(glib_CFLAGS) \
			$(gio_CFLAGS) \
			$(gio_unix_CFLAGS)

DEFS += -D_BSD_SOURCE -D_POSIX_C_SOURCE=2
