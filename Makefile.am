bin_PROGRAMS = notifyserv
dist_man_MANS = notifyserv.1

notifyserv_SOURCES =	src/misc.c src/misc.h \
			src/notifyserv.c src/notifyserv.h \
			src/notify-irc.c src/notify-irc.h

notifyserv.1: src/notifyserv.c
		$(MAKE) $(AM_MAKEFLAGS) notifyserv$(EXEEXT)
		help2man --name="IRC notification system" --no-info --output=notifyserv.1 --version-option="-V" ./notifyserv$(EXEEXT)

DEFS += -D_GNU_SOURCE
