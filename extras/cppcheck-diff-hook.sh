#!/bin/bash

HG_ROOT=`hg root`

if [ x"$HG_ROOT" == x"" ] ; then
	echo "not an hg repo..."
	exit 1
fi

pushd ${HG_ROOT} > /dev/null

#Get a list of modified files
ALTERED_FILES=`hg status | egrep '^[MA]' | egrep '\.(cpp|h)$' | sed 's/[MA]\s//'` 

TMPDIR=/tmp/cppcheck-newerrs/

mkdir -p "$TMPDIR"

SAVEIFS=$IFS
IFS=$(echo -en "\n\b")
for i in $ALTERED_FILES
do
	echo "Checking for errors :  $i"
	hg diff "$i" > $TMPDIR/patch-file-dhje
	mkdir -p `dirname "$TMPDIR/$i"`
	cp "$i" "$TMPDIR/$i"
	cp "$i" "$TMPDIR/$i.orig"

	pushd $TMPDIR > /dev/null
	patch -Rs -p1 < patch-file-dhje
	cppcheck-diff.sh "$i" "$i.orig" >> "$TMPDIR/cppcheck-new-errs"
	rm "$i" "$i.orig" patch-file-dhje
	popd > /dev/null


done

popd > /dev/null

if [ ! -f $TMPDIR/cppcheck-new-errs ] ; then
	exit 0
fi

if [ `wc -l $TMPDIR/cppcheck-new-errs | awk '{print $1}' ` -ne 0 ] ; then
	echo "The following new cppcheck warnings were detected:"
	cat "$TMPDIR/cppcheck-new-errs"

	echo "abort? (y/n)"

	read wantAbort
	
	if [ x"$wantAbort" == x"y" ] ; then
		rm -rf "$TMPDIR"
		exit 1
	fi
fi
rm -rf "$TMPDIR"

