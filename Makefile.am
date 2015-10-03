ACLOCAL_AMFLAGS = -I m4
SUBDIRS = scripts src man conf
EXTRA_DIST = autogen.sh ocb-license.html README.md COPYING.iOS
BUILT_SOURCES = version.h
CLANG_SCAN_BUILD = scan-build
AM_DISTCHECK_CONFIGURE_FLAGS = --enable-compile-warnings=distcheck --enable-examples

.PHONY:	VERSION

VERSION:
	@echo @PACKAGE_STRING@ > VERSION.dist
	@set -e; if git describe --dirty --always > VERSION.git 2>&1 && \
		[ -z `git rev-parse --show-prefix` ]; then \
		if ! diff -q VERSION.git VERSION > /dev/null 2>&1; then \
			mv -f VERSION.git VERSION; \
		fi; \
	elif ! diff -q VERSION.dist VERSION > /dev/null 2>&1; then \
		mv -f VERSION.dist VERSION; \
	fi
	@rm -f VERSION.dist VERSION.git

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
		cov-build --dir cov-int $(MAKE) check && \
		tar -caf mosh-coverity.txz cov-int

scan-build: $(BUILT_SOURCES) config.h
	$(MAKE) clean && $(CLANG_SCAN_BUILD) $(MAKE) check
