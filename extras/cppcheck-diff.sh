#!/bin/bash
# Hook script for checking two files, running cppcheck
#  and looking for new cppcheck output

if [ $# -ne 2 ] ; then
	echo "usage: original.cpp altered.cpp"
	exit 1
fi

#Check for missing programs
#---
NEEDED_PROGRAMS="tre-agrep diff"
MISSING_PROG=0
for i in $NEEDED_PROGRAMS 
do
	if [ x`which $i` == x""  ] ; then
		echo "Missing program : " $i
		MISSING_PROG=1
	fi
done

if [ $MISSING_PROG -ne 0 ] ; then
	exit 1;
fi
#---

TMPDIR=/tmp

FILTER_LINE_FILE=$TMPDIR/cppcheck-ignore
echo 'does not have a constructor
(information)
is not initialized in the constructor
C-style pointer casting
[Aa]ssert
convertion between' > $FILTER_LINE_FILE


DIFFTMP=$TMPDIR/diff-tmp

BASETMP=$TMPDIR/cppcheck-tmp

#run cppcheck on both files
cppcheck -q --enable=style,performance,portability --inconclusive $1 2>&1| grep -v -f $FILTER_LINE_FILE > ${BASETMP}-a 
cppcheck -q --enable=style,performance,portability --inconclusive $2 2>&1 | grep -v -f $FILTER_LINE_FILE > ${BASETMP}-b

#normalise names
sed -i "s@$1@filename@" ${BASETMP}-a
sed -i "s@$2@filename@" ${BASETMP}-b

#create backups
cp $BASETMP-a $BASETMP-a.orig
cp $BASETMP-b $BASETMP-b.orig

# Strip line numbers 
# Cppcheck can output multiple line numbers, so cut them all
sed  -i 's/:[0-9]*\]/:]/g' ${BASETMP}-a
sed  -i 's/:[0-9]*\]/:]/g' ${BASETMP}-b


diff -uh ${BASETMP}-a ${BASETMP}-b | egrep '^\+' | egrep -v '^\+\+' | sed 's/^+//' > $BASETMP-diff

#perform a near-string match, disallowing subsitution, or erase, 
# to  restore the file lines
#---
SAVEIFS=$IFS
IFS=`echo -en "\n\b"`
rm -f $BASETMP-matches
for i in `cat $BASETMP-diff`
do
	tre-agrep  -k -7 -D 100 -S 100  "$i" $BASETMP-b.orig  >> $BASETMP-matches
done
IFS=$SAVEIFS
#---

if [ -f $BASETMP-matches ]  ; then
	sed -i "s@filename@$1@" $BASETMP-matches
	cat $BASETMP-matches
fi

