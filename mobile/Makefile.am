SUBDIRS = src

user_install:
	if [ ! -d $(HOME)/.irssi/modules ]; then mkdir -p $(HOME)/.irssi/modules; fi
	cp -f src/.libs/libmobile.so $(HOME)/.irssi/modules/

EXTRA_DIST = \
	irssiwap.php
