#!/bin/bash


#Location of HG repository to open
REPOSITORY_PATH=
CHECKOUT_PATH=
BINARY=./src/3Depict
PROJECTNAME=3Depict

#Abort note file
ABORT_MSG_FILE=NOTE_ABORT

#Number of simultaneous processes when building
NUM_PROC=2 

notifyAbort()
{
	echo `cat $ABORT_MSG_FILE`
	#TODO: Do we have a reliable SMTP system that we know how to use??
	
		
	rm $ABORT_MSG_FILE

	echo "$CHECKOUT_PATH may need to be removed before re-running this script"
	exit 1
}

#-- Check variables are set --
# TODO: This is not neat, but I don't have internet and don't think
# we can loop over empty items.
NOTCONFIGURED=0
if [ x$REPOSITORY_PATH == x""  ] ; then
	NOTCONFIGURED=1
fi
if [ x$CHECKOUT_PATH == x""  ] ; then
	NOTCONFIGURED=1
fi
if [ x$BINARY == x"" ] ; then
	NOTCONFIGURED=1
fi
if [ x$PROJECTNAME == x""  ] ; then
	NOTCONFIGURED=1
fi
if [ $NOTCONFIGURED -ne 0 ] ; then
	echo "PATHS NOT CONFIGURED -- CONFIGUREPATHS FIRST, THEN DELETE THIS ERROR MESSAGE"
	exit 1
fi
#------

if [ x`which hg` == x"" ] ; then
	echo "hg binary not available in $PATH. Aborting" > $ABORT_MSG_FILE
	notifyAbort
fi

if [ -d $CHECKOUT_PATH ] ; then
	echo "target destination for checkout ($CHECKOUT_PATH) not empty!" > $ABORT_MSG_FILE
	notifyAbort
fi

mkdir -p $CHECKOUT_PATH

pushd $CHECKOUT_PATH

hg clone $REPOSITORY_PATH

if [ $? -ne 0 ] ; then
	echo "repository checkout failed!" > $ABORT_MSG_FILE
	notifyAbort
fi

if [ ! -d $PROJECTNAME ] ; then
	echo "Odd, 3depict checked out, but no code dir found" > $ABORT_MSG_FILE
	notifyAbort
fi

cd $PROJECTNAME

#Initiate the configuration
./configure --enable-debug-checks --enable-openmp-parallel

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

#Run the unit tests
$BINARY -t

if [ $? -ne 0 ] ; then
	echo "Unit test failure! Fix it Fix it Fix it! Fix it! " > $ABORT_MSG_FILE
	exit 1
fi
popd

rm -rf $CHECKOUT_PATH

