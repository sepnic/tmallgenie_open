if ENABLE_NOPOLL_DOC
NOPOLL_DOC_DIR = doc
endif

SUBDIRS = src $(NOPOLL_DOC_DIR)
EXTRA_DIST = nopoll.pc.in VERSION Makefile.win
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = nopoll.pc

all-local: update-defs

update-defs:
	cd src; make update-def; cd ..

