ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src scripts man conf
EXTRA_DIST = autogen.sh ocb-license.html README.md COPYING.iOS
nodist_noinst_HEADERS = version.h
BUILT_SOURCES = version.h
CLANG_SCAN_BUILD = scan-build

.PHONY:	VERSION

VERSION:
	@set -e; if [ ! -f VERSION ]; then echo @PACKAGE_STRING@ > VERSION; fi
	@set -e; if git status > /dev/null 2>&1; then \
		git describe --dirty > VERSION.new;  \
		if ! diff -q VERSION VERSION.new > /dev/null 2>&1; then \
			mv -f VERSION.new VERSION; \
		fi; \
	fi
	@rm -f VERSION.new

version.h:	VERSION
	@printf '#define BUILD_VERSION "%s"\n' "$$(cat VERSION)" > version.h.new
	@set -e; if ! diff -q version.h version.h.new > /dev/null 2>&1; then \
		mv -f version.h.new version.h; \
	fi
	@rm -f version.h.new

clean-local:
	@rm -rf version.h VERSION cov-int mosh-coverity.txz

cppcheck: $(BUILT_SOURCES) config.h
	cppcheck --enable=all --template=gcc -include config.h -I . \
		-I src/crypto -I src/frontend -I src/network -I src/protobufs \
		-I src/statesync -I src/terminal -I src/util \
		-I /usr/include -I /usr/include/google/protobuf -I/usr/include/openssl \
		.

cov-build: $(BUILT_SOURCES) config.h
	$(MAKE) clean && rm -rf cov-int && \
		cov-build --dir cov-int $(MAKE) && \
		tar -caf mosh-coverity.txz cov-int

scan-build: $(BUILT_SOURCES) config.h
	$(MAKE) clean && $(CLANG_SCAN_BUILD) $(MAKE)
