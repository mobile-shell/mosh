ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src scripts man conf
EXTRA_DIST = autogen.sh ocb-license.html README.md COPYING.iOS
nodist_noinst_HEADERS = version.h
BUILT_SOURCES = version.h

.PHONY:	VERSION

VERSION:
	@-[ -f VERSION ] || echo @PACKAGE_STRING@ > VERSION
	@-git status > /dev/null 2>&1 && \
		git describe --dirty > VERSION.new && \
		! diff -q VERSION VERSION.new > /dev/null && \
		mv VERSION.new VERSION
	@rm -f VERSION.new

version.h:	VERSION
	@printf '#define BUILD_VERSION "%s"\n' "$$(cat VERSION)" > version.h.new
	@-! diff -q version.h version.h.new > /dev/null && \
		mv version.h.new version.h
	@rm -f version.h.new

clean-local:
	@rm -f version.h VERSION
