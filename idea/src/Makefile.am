plugindir = $(libdir)/irssi/modules

INCLUDES = $(GLIB_CFLAGS) -I$(IRSSI_INCLUDE) -I$(IRSSI_INCLUDE)/src \
	-I$(IRSSI_INCLUDE)/src/core -I$(IRSSI_INCLUDE)/src/fe-common/core

plugin_LTLIBRARIES = libidea.la

libidea_la_LDFLAGS = -avoid-version

libidea_la_SOURCES = \
	plugin.c \
	module-formats.c \
	irc_api.c \
	crc32.c \
	irc_crc.c \
	irc_crypt.c \
	irc_idea_v1.c \
	irc_idea_v2.c \
	irc_idea_v3.c \
	irc_b64.c \
	idea.c

noinst_HEADERS = \
	module.h \
	module-formats.h \
	idea.h \
	crc32.h \
	irc_crypt.h

EXTRA_DIST = \
	COPYRIGHT.irc_crypt \
	README.irc_crypt
