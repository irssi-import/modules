moduledir = $(libdir)/irssi/modules

INCLUDES = $(GLIB_CFLAGS) -I$(IRSSI_INCLUDE) -I$(IRSSI_INCLUDE)/src

module_LTLIBRARIES = libmobile.la

libmobile_la_LDFLAGS = -avoid-version

INCLUDES = \
	$(GLIB_CFLAGS) \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-I$(IRSSI_INCLUDE) -I$(IRSSI_INCLUDE)/src \
	-I$(IRSSI_INCLUDE)/src/core

libmobile_la_SOURCES = \
	mobile.c

noinst_HEADERS = \
	module.h
