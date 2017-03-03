#!/bin/sh
#
# Cleanup for Appveyor peculiarities.
#

# Echo, eval, and error on shell commands.
eeval()
{
    echo "$0: $*" >&2
    eval "$@"
    rv=$?
    if [ $rv -ne 0 ]; then
	echo "$0: failed, exitcode $rv"
	exit $rv
    fi
    return 0
}

# We inherit a broken Windows path with a Windows Git.
PATH=/bin

# This supposedly fixes some failures.
exec 0</dev/null

# Mosh-specific environment
export LANG=en_US.UTF-8

case $1 in
    before_build)
	# This repo was checked out with Windows Git, repair it for Cygwin.
	eeval git config --local core.symlinks true
	eeval git clean --force --quiet -x -d
	eeval git reset --hard
	;;
    build_script)
	eeval ./autogen.sh
	eeval ./configure --enable-compile-warnings=error --enable-examples
	eeval make distcheck VERBOSE=1 V=1
	;;
    wait)
	touch wait.lck
	while [ -f wait.lck ]; do
	    sleep 10
	done
	;;
    *)
	echo "Fail: $0 $*"
	exit 2
	;;
esac
