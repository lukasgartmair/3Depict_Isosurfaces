#!/bin/bash

#Reconfigure
./configure
make distclean

if [ $? -ne 0 ] ; then
	echo "Something went wrong with distclean. Cannot continue"
	exit 1
fi

./configure
if [ $? -ne 0 ] ; then
	echo "Something went wrong with configure. Cannot continue"
	exit 1
fi
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

#Check version number in basics.cpp is set concomitantly.

if [ x"`grep PROGRAM_VERSION src/basics.cpp | grep $VER`" == x"" ] ; then
	echo " WARNING: Program version not set to match between configure.ac. and basics.cpp"
fi


rm -rf tarball
mkdir tarball


cd tarball/
tar -zxf ../3depict-$VER.tar.gz
rm ../3depict-$VER.tar.gz
#Autoconf buggers up the name case
mv 3depict-$VER 3Depict-$VER

#--- Translation stuff--

#Build the translation files
pushd ../translations
./makeTranslations 
mkdir -p ../tarball/3Depict-$VER/translations
cp 3Depict*.mo ../tarball/3Depict-$VER/translations/
popd


pushd 3Depict-$VER/translations/

#Move the .mo file to the correct subdir
for i in `ls *.mo`
do
	LOCALEVAL=`basename $i | sed 's/\.mo//' | sed 's/3Depict_//'`

	mkdir -p locales/$LOCALEVAL/LC_MESSAGES
	mv $i locales/$LOCALEVAL/LC_MESSAGES/3Depict.mo
done

if [ -d locales ] ; then
	mv locales ../
else
	echo "ERROR: No Locales built.. Aborting"
	exit 1
fi

popd

#--------------



#make the final source tarball
tar -cz 3Depict-$VER > 3Depict-$VER.tar.gz


