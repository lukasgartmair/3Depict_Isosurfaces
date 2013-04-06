#!/bin/bash

PROGRAM_NAME=3Depict
#Pull version number out of configure
VERSION=`cat configure.ac | grep '^\s*AC_INIT(' | awk -F, '{ print $2 } ' | sed 's/\s*\[//' | sed 's/\]\s*//'
BUILT_PROGRAMS_DIR=.
INSTALL_ROOT=./packaging/
MAC_OS_VER=`sw_vers | grep ProductVersion | awk '{print $2}'`

cp makeMacOSXApp ../..
cp -R ${PROGRAM_NAME}.app ../..
cd ../..

#Set bundle version
sed -s  "s/BUNDLE_VERSION/$VERSION/" ./3Depict.app/Contents/Info.plist 

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

#--

ARCHIVE_FILENAME="${BUILT_PROGRAMS_DIR}/${PROGRAM_NAME}-${VERSION}-${MAC_OS_VER}.pkg"
pkgbuild --root "${INSTALL_ROOT}" \
    --identifier "net.sourceforge.threedepict.${PROGRAM_NAME}" \
    --version "$VERSION" \
    --install-location "/Applications" \
    ${ARCHIVE_FILENAME}
