#!/bin/sh

# Major version number has been 1 forever, only compare minor
version=`aclocal --version | head -n1 | grep -Eo '[0-9.]+'`
if [ `echo $version | cut -d'.' -f2` -lt 10 ]; then
	cat << EOF
Mosh requires automake version >= 1.10 (detected version $version) as it uses
the --install flag to aclocal. Exiting...
EOF
	exit 1
fi

exec autoreconf -fi
