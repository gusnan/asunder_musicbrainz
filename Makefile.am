## Process this file with automake to produce Makefile.in

SUBDIRS = src po

pixdir = $(datadir)/pixmaps
pix_DATA = pixmaps/asunder.png pixmaps/asunder.svg

EXTRA_DIST = asunder.desktop pixmaps/asunder.png pixmaps/asunder.svg syslogng

ACLOCAL_AMFLAGS = -I m4

desktopdir=$(datadir)/applications

desktop_DATA = asunder.desktop

install-data-hook:
	cd syslogng && $(MAKE) install

uninstall-hook: 
	cd syslogng && $(MAKE) uninstall
