#!/bin/bash

COVERITY_BASEDIR=~/sources/cov-analysis-linux64-6/cov-analysis-linux64-6.6.1/
#Replace with real key.
COVERITY_KEY=QwQHOG5L3u53VhhdwKq6GA
COVERITY_EMAIL_ID=email at email.com
COVERITY_PROJECT=3depict

THREEDEPICT_VERSION="0.0.18~rc1"

PROJECT_BASEDIR=~/code/3Depict/


#Internal variables
COVERITY_TMPSCRIPT=./coverity-build.sh

function cov_build()
{
	pushd $PROJECT_BASEDIR

	#Current version of coverity doesn't work with gcc > 4.6
	cat > $COVERITY_TMPSCRIPT << "EOF"
#!/bin/bash
export CXX=gcc
export CC=gcc
export LDFLAGS=-lstdc++
./configure --disable-ubsan || { exit 1; }
make clean || { exit 1; }
make -j4 || { exit 1; }
EOF
	chmod u+x  $COVERITY_TMPSCRIPT

	#Use coverity build wrapper to build the project
	#note that coverity's upload system is quite dumb,
	# and expects to find /cov-int/build-log.txt
	# in the tarball. It will not search for it
	COVERITY_UPLOAD_DIR=cov-int 
	cov-build --dir $COVERITY_UPLOAD_DIR $COVERITY_TMPSCRIPT || { exit 1; }

	if [ ! -f ${COVERITY_UPLOAD_DIR}/build-log.txt ] ; then
		echo "Something went wrong. Expected to find ${COVERITY_UPLOAD_DIR}/build-log.txt, but didn't"
		exit 1
	fi

	tar -cz $COVERITY_UPLOAD_DIR > cov-upload.tar.gz || { echo "tarballing failed" ; exit 1; }


	echo "----------------"
	echo "NOTICE : UPLOADABLE TARBALL BUILT :" cov-upload.tar.gz
	echo "\twill now attempt submission"
	echo "----------------"
	rm build.sh

	sleep 2


	popd
}

function verify_environment()
{
	DOEXIT=0;
	if [ ! -d $PROJECT_BASEDIR ] ; then
		echo "Project dir not found. Looking for $PROJECT_BASEDIR"
		DOEXIT=1
	fi

	if [ x`which curl` == x"" ] ; then
		echo "Couldn't find curl."
		DOEXIT=1
	fi

	if [ ! -d $COVERITY_BASEDIR ] ; then
		echo "Coverity base dir missing. Looking for $COVERITY_BASEDIR"
		DOEXIT=1
	fi

	if [ ! -f ${COVERITY_BASEDIR}/bin/cov-build ] ; then
		echo "Coverity tool \"cov-build\" not found. Expected ${COVERITY_BASEDIR}/bin/cov-build"
		DOEXIT=1
	fi

	if [ x"$COVERITY_KEY" == x"" ] ; then
		echo "Coverity key not set. Please edit script to set"
		DOEXIT=1
	fi

	IP_TEST=8.8.8.8
	ping -c 1 $IP_TEST
	if [ $? -ne 0 ] ; then
		echo "Unable to ping $IP_TEST : is your interwebs working? We need this to send the results to coverity"
		DOEXIT=1
	fi
	

	if [ $DOEXIT -ne 0 ] ; then
		exit 1;
	fi
}

function cov_submit()
{
 curl   --form project=$COVERITY_PROJECT\
	--form token=$COVERITY_KEY\
	--form email=$COVERITY_EMAIL_ID\
	--form version=$THREEDEPICT_VERSION\
	--form description="Coverity scan upload"\
	--form file=@../cov-upload.tar.gz \
	https://scan.coverity.com/builds?project=3depict
}

export PATH=${PATH}:${COVERITY_BASEDIR}/bin/

echo "Verfying environment..."
verify_environment
echo "Doing the build...."
cov_build
echo "Submitting..."
cov_submit
