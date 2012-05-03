#!/bin/bash

set -e

echo "Building into prefix..."

PREFIX=`pwd`/prefix
PREFIX_i386=`pwd`/prefix_i386
PREFIX_x86_64=`pwd`/prefix_x86_64
#PREFIX_ppc=`pwd`/prefix_ppc
#PREFIX_ppc64=`pwd`/prefix_ppc64

mkdir -p "$PREFIX"
mkdir -p "$PREFIX_i386"
mkdir -p "$PREFIX_x86_64"
#mkdir -p "$PREFIX_ppc"
#mkdir -p "$PREFIX_ppc64"

pushd .. > /dev/null

if [ ! -f configure ];
then
    echo "Running autogen."
    PATH=/opt/local/bin:$PATH ./autogen.sh
fi

echo "Building for x86_64..."
./configure --prefix="$PREFIX_x86_64" ac_cv_poll_pty=no \
    CC="clang -arch x86_64" CPP="clang -arch x86_64 -E" CXX="clang++ -arch x86_64" \
    TINFO_LIBS=-lncurses protobuf_LIBS=/opt/local/lib/libprotobuf.a
make clean
make install -j8

echo "Building for i386..."
./configure --prefix="$PREFIX_i386" ac_cv_poll_pty=no \
    CC="clang -arch i386" CPP="clang -arch i386 -E" CXX="clang++ -arch i386" \
    TINFO_LIBS=-lncurses protobuf_LIBS=/opt/local/lib/libprotobuf.a
make clean
make install -j8

#echo "Building for ppc..."
#./configure --prefix="$PREFIX_ppc" ac_cv_poll_pty=no \
#    --target=ppc-apple-darwin --build=i686-apple-darwin --host=ppc-apple-darwin \
#    CC="clang -arch ppc -mmacosx-version-min=10.5" CPP="clang -arch ppc -mmacosx-version-min=10.5 -E" CXX="clang++ -arch ppc -mmacosx-version-min=10.5" \
#    TINFO_LIBS=-lncurses protobuf_LIBS=/opt/local/lib/libprotobuf.a
#make clean
#make install -j8
#
#echo "Building for ppc64..."
#./configure --prefix="$PREFIX_ppc64" ac_cv_poll_pty=no \
#    --target=ppc-apple-darwin --build=i686-apple-darwin --host=ppc-apple-darwin \
#    CC="clang -arch ppc64 -mmacosx-version-min=10.5" CPP="clang -arch ppc64 -mmacosx-version-min=10.5 -E" CXX="clang++ -arch ppc64 -mmacosx-version-min=10.5" \
#    TINFO_LIBS=-lncurses protobuf_LIBS=/opt/local/lib/libprotobuf.a
#make clean
#make install -j8

echo "Building universal binaries..."

cp -r "$PREFIX_x86_64/" "$PREFIX/"

strip "$PREFIX_i386/bin/mosh-client"
strip "$PREFIX_i386/bin/mosh-server"
strip "$PREFIX_x86_64/bin/mosh-client"
strip "$PREFIX_x86_64/bin/mosh-server"
#strip "$PREFIX_ppc/bin/mosh-client"
#strip "$PREFIX_ppc/bin/mosh-server"
#strip "$PREFIX_ppc64/bin/mosh-client"
#strip "$PREFIX_ppc64/bin/mosh-server"

#lipo -create "$PREFIX_ppc/bin/mosh-client" "$PREFIX_ppc64/bin/mosh-client" "$PREFIX_i386/bin/mosh-client" "$PREFIX_x86_64/bin/mosh-client" -output "$PREFIX/bin/mosh-client"
#lipo -create "$PREFIX_ppc/bin/mosh-server" "$PREFIX_ppc64/bin/mosh-server" "$PREFIX_i386/bin/mosh-server" "$PREFIX_x86_64/bin/mosh-server" -output "$PREFIX/bin/mosh-server"
lipo -create "$PREFIX_i386/bin/mosh-client" "$PREFIX_x86_64/bin/mosh-client" -output "$PREFIX/bin/mosh-client"
lipo -create "$PREFIX_i386/bin/mosh-server" "$PREFIX_x86_64/bin/mosh-server" -output "$PREFIX/bin/mosh-server"

perl -wlpi -e 's{#!/usr/bin/env perl}{#!/usr/bin/perl}' "$PREFIX/bin/mosh"

popd > /dev/null

echo "Preprocessing package description..."
PACKAGE_VERSION=`grep PACKAGE_VERSION ../config.h | sed -e 's/^.*"\(.*\)"$/\1/'`

INDIR=mosh-package.pmdoc.in
OUTDIR=mosh-package.pmdoc
OUTFILE="Mosh $PACKAGE_VERSION.pkg"

mkdir -p "$OUTDIR"

pushd "$INDIR" > /dev/null

for file in *
do
    sed -e 's/$PACKAGE_VERSION/'"$PACKAGE_VERSION"'/g' "$file" > "../$OUTDIR/$file"
done

popd > /dev/null

echo "Running PackageMaker..."
PATH=/Applications/PackageMaker.app/Contents/MacOS:$PATH PackageMaker -d mosh-package.pmdoc -o "$OUTFILE"

echo "Cleaning up..."
rm -r "$OUTDIR"

if [ -f "$OUTFILE" ];
then
    echo "Successfully built $OUTFILE."
else
    echo "There was an error building $OUTFILE."
fi
