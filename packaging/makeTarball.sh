#!/bin/bash

#Reconfigure
make distclean
./configure
rm -rf autom4te.cache

#Build tarball
make dist

#Check version
VER=`ls 3depict-*gz | sed 's/^3depict-\([0-9\.]*\).tar.gz$/\1/' `
echo "Version is apparently :" $VER
if [ x`grep $VER .hgtags` == x""  ] ; then
	echo " NOTICE: version number not seen in HG file..."
else
	echo " NOTICE: Version number exists in HG"
fi


rm -rf tarball
mkdir tarball


cd tarball/
tar -zxf ../3depict-$VER.tar.gz
rm ../3depict-$VER.tar.gz

#Autoconf buggers up the name case
mv 3depict-$VER 3Depict-$VER

tar -cz 3Depict-$VER > 3Depict-$VER.tar.gz





