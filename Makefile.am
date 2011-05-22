bin_PROGRAMS = notifyserv
dist_man_MANS = notifyserv.1

notifyserv_SOURCES =	src/notifyserv.c src/notifyserv.h \
			src/irc.c src/irc.h \
			src/listen.c src/listen.h \
			src/misc.c src/misc.h

notifyserv_LDADD =	$(glib_LIBS) \
			$(gio_LIBS)

notifyserv_CFLAGS =	$(glib_CFLAGS) \
			$(gio_CFLAGS)

notifyserv.1: notifyserv
		help2man --name="IRC notification system" --no-info --output=notifyserv.1 --version-option="-V" ./notifyserv$(EXEEXT)

clean:	clean-am
	rm -f notifyserv.1

DEFS += -D_BSD_SOURCE -D_POSIX_C_SOURCE=2
