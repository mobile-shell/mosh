#!/bin/bash

echo "Building into prefix..."

mkdir -p prefix

pushd .. > /dev/null

if [ ! -f configure ];
then
    echo "Running autogen."
    ./autogen.sh
fi

CONFIG_ARGS=--prefix=`pwd`/macosx/prefix

if [ -f config.log ];
then
    grep -e "$CONFIG_ARGS" config.log > /dev/null || ./configure "$CONFIG_ARGS"
else
    ./configure "$CONFIG_ARGS"
fi

make install -j8

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
/Applications/PackageMaker.app/Contents/MacOS/PackageMaker -d mosh-package.pmdoc -o "$OUTFILE"

echo "Cleaning up..."
rm -r "$OUTDIR"

if [ -f "$OUTFILE" ];
then
    echo "Successfully built $OUTFILE."
else
    echo "There was an error building $OUTFILE."
fi
