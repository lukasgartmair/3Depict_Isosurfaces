#!/bin/bash

PROGRAM_NAME=3Depict

cp makeMacOSXApp ../..
cp -R ${PROGRAM_NAME}.app ../..
cd ../..
make distclean > out.txt 2>&1

#Pull version number out of configure
VERSION=`cat ./configure.ac | grep '^\s*AC_INIT(' | awk -F, '{ print $2 } ' | sed 's/\s*\[//' | sed 's/\]\s*//' | sed 's/\ //g'`
BUILT_PROGRAMS_DIR=.
MAC_OS_VER=`sw_vers | grep ProductVersion | awk '{print $2}'`
YEAR=`date | awk '{print $NF}'`

#Fix some semi-constant values
#===
INFO_PLIST="./3Depict.app/Contents/Info.plist"
#Set program version
sed -i "s/BUNDLE_VERSION/${VERSION}/" ${INFO_PLIST}
#Set year
sed -i "s/3Depict Copyright (C) 20[123][0-9]/${DATEYEAR}/" ${INFO_PLIST}
#Set minimum OS numbber
sed -i "s/MIN_OS_VER/${MAC_OS_VER}/"  ${INFO_PLIST}
#===

time ./makeMacOSXApp --update-config=yes --parallel=yes --debug=no > out.txt 2>&1
if [ x"`tail -1 out.txt`" == x"Done" ] ; then
	echo "Finished compiling"
else
	echo "Failed"
	vi out.txt
	exit 1
fi


#Perform some QA checks
#--
#TODO: Check debug symbols are stripped

#Ensure that program name not screwed up by XCode
if [ x`grep ${INFO_PLIST} -Depict` != x"" ] ; then
	echo "Program name incorrectly stored in package, is listed as _Depict, rather than $PROGRAM_NAME"
	exit 1
fi

#--

ARCHIVE_FILENAME=`echo "${BUILT_PROGRAMS_DIR}/${PROGRAM_NAME}-${VERSION}-${MAC_OS_VER}.pkg"`
pkgbuild --root "${PROGRAM_NAME}.app" \
    --identifier "net.sourceforge.threedepict.${PROGRAM_NAME}" \
    --version "${VERSION}" \
    --install-location "/Applications/${PROGRAM_NAME}.app" \
    ${ARCHIVE_FILENAME}
