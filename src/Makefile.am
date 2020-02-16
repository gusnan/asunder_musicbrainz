## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = asunder

asunder_SOURCES = \
	main.c main.h \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	prefs.c prefs.h \
	threads.c threads.h \
	util.c util.h \
	wrappers.c wrappers.h \
	completion.c completion.h

asunder_LDADD = @PACKAGE_LIBS@ $(INTLLIBS)
