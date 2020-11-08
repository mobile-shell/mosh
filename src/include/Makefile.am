BUILT_SOURCES = version.h
nodist_INCLUDES = version.h

clean-local:
	@rm -rf version.h

version.h:	../../VERSION
	@test -s $<
	@printf '#define BUILD_VERSION "%s"\n' "$$(cat $<)" > $@.new
	@set -e; if ! diff -q $@ $@.new > /dev/null 2>&1; then \
		mv -f $@.new $@; \
	fi
	@rm -f $@.new

