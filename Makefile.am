bin_PROGRAMS = notifyserv
dist_man_MANS = notifyserv.1

notifyserv_SOURCES =	src/notifyserv.c src/notifyserv.h \
			src/irc.c src/irc.h \
			src/listen.c src/listen.h \
			src/log.c src/log.h

notifyserv_LDADD =	$(glib_LIBS) \
			$(gio_LIBS) \
			$(gio_unix_LIBS)

notifyserv_CFLAGS =	$(glib_CFLAGS) \
			$(gio_CFLAGS) \
			$(gio_unix_CFLAGS)

notifyserv.1: notifyserv
		help2man --name="IRC notification system" --no-info --output=notifyserv.1 --version-option="-V" ./notifyserv$(EXEEXT)

clean:	clean-am
	rm -f notifyserv.1

DEFS += -D_BSD_SOURCE -D_POSIX_C_SOURCE=2
