#!/bin/sh
#
# Cleanup for Appveyor peculiarities.
#
# This thing is dangerous.  See "rm -rf *".  It tries hard, but if it
# gets it wrong you will not be happy.
#
# Only run this on an Appveyor instance, where the consequences are
# nil.
#

# Echo, eval, and error on shell commands.
eeval()
{
    echo "$0: $*" >&2
    eval "$@"
    rv=$?
    if [ $rv -ne 0 ]; then
	echo "$0: failed, exitcod $rv"
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

# Make sure we're on Appveyor
if [ -z "$APPVEYOR_BUILD_FOLDER" ]; then
    echo "$0: APPVEYOR_BUILD_FOLDER variable empty" >&2
    exit 2
fi

# Make really, really sure we're in the build dir
cd $APPVEYOR_BUILD_FOLDER || exit 2
if [ "$(pwd)" = "$HOME" -o "$PWD" = "$HOME" ]; then
    echo "$0: in home directory" >&2
    exit 2
fi

set -e

case $1 in
    before_build)
	# Check that we're in a Mosh repo with mosh-1.2.5, then clean it mercilessly
	eeval git config --local core.symlinks true
	eeval "git describe 3c3b356cb5e387887499beb2eedddce185f36944 && rm -rf * && git reset --hard"
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
