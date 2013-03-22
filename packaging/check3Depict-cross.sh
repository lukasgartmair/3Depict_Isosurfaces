#!/bin/bash
#A script for checking that the cross-compilation build still compiles

#You probably want to set up your own environment first, then customise
# this script, rather than trying to use it as a base for cross-compilation

CROSS_BASEDIR=
#Abort notice file
ABORT_MSG_FILE=/tmp/NOTE_ABORT
#Project directory, relative to CROSS_BASEDIR
PROJECTDIR=3Depict

if [ ! -d $CROSS_BASEDIR ] ; then
	echo "Cross compilation base dir missing :" $CROSS_BASEDIR
	exit 1
fi

if [ ! -d $CROSS_BASEDIR/$PROJECT ] ; then
	echo "Cross compilation base dir missing :" $CROSS_BASEDIR
	exit 1
fi

#Number of simultaneous processes when building
NUM_PROC=3 

notifyAbort()
{
	cat $ABORT_MSG_FILE
	rm $ABORT_MSG_FILE
	exit 1
}

cd $CROSS_BASEDIR/$PROJECT
#Set the cross compilation path
PATH=$CROSS_BASEDIR/bin:$PATH

hg pull

if [ $? -ne 0 ] ; then
	echo "Pull failed!"  >> $ABORT_MSG_FILE
	notifyAbort
fi

hg up -r tip

if [ $? -ne 0 ] ; then
	echo "Update failed!"  >> $ABORT_MSG_FILE
	notifyAbort
fi


make distclean
if [ $? -ne 0 ] ; then
	echo "cross compile did not clean!" > $ABORT_MSG_FILE
	notifyAbort
fi

#Initiate the configuration
export CPPFLAGS="-I${CROSS_BASEDIR}/include/ -DUNICODE"
export LDFLAGS=-L${CROSS_BASEDIR}/lib/
#openmp is, out of the box, not supported by the cross-compiler.
# Bug : http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=625779
# Looks like its an easy fix, but we would need our own private version
./configure --host=x86_64-w64-mingw32 --enable-debug-checks 

if [ $? -ne 0 ] ; then
	echo "Configure failed!" > $ABORT_MSG_FILE
	notifyAbort
fi



make -j $NUM_PROC

if [ $? -ne 0 ] ; then 
	echo "Build failed" > $ABORT_MSG_FILE
	notifyAbort
fi

if [ ! -f $BINARY ] ; then  
	echo "Weird, binary is missing!" > $ABORT_MSG_FILE
	notifyAbort
fi

echo "Build succeeded"

