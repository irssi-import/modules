SUBDIRS = src

user_install:
	if [ ! -d $(HOME)/.irssi/modules ]; then mkdir -p $(HOME)/.irssi/modules; fi
	cp -f src/.libs/libidea.so $(HOME)/.irssi/modules/

EXTRA_DIST = \
	debian/changelog \
	debian/control \
	debian/copyright \
	debian/rules
