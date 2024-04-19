AM_CPPFLAGS = 		\
	@KDRIVE_INCS@	\
	@KDRIVE_CFLAGS@

noinst_LIBRARIES = libwscons.a

bin_PROGRAMS = Xwscons

libwscons_a_SOURCES =	\
	wsfb.c		\
	wsdev.h		\
	keyboard.c	\
	mouse.c		\
	wsconskbd.h

Xwscons_SOURCES =	\
	wsconsinit.c

Xwscons_LDADD = 	\
	libwscons.a	\
	@KDRIVE_LIBS@	\
	@XSERVER_LIBS@					

Xwscons_DEPENDENCIES =					\
	libwscons.a @KDRIVE_LIBS@

install-exec-hook:
if INSTALL_SETUID
	chown root $(DESTDIR)$(bindir)/Xwscons
	chmod u+s $(DESTDIR)$(bindir)/Xwscons
endif
