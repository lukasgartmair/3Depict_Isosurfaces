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

#Try to locate the corresponding wxMO file, then install it
for i in `ls *.mo`
do
	LOCALEVAL=`basename $i | sed 's/\.mo//' | sed 's/3Depict_//'`
	WXMO=`find /usr/share/locale/$LOCALEVAL -name wxstd.mo`
		
	mkdir -p LC_MESSAGES/$LOCALEVAL
	mv $i LC_MESSAGES/$LOCALEVAL/3Depict.mo

	#Did we find a wxstd.mo for this locale?
	if [ x"$WXMO" == x"" ] ; then
		echo "WARNING: No wxstd.mo found for locale: $LOCALEVAL "
	else
		echo  cp $WXMO LC_MESSAGES/$LOCALEVAL/
	fi
done

if [ -d LC_MESSAGES ] ; then
	mv LC_MESSAGES ../
else
	echo "ERROR: No Locales built.. Aborting"
	exit 1
fi

popd

#--------------



#make the final source tarball
tar -cz 3Depict-$VER > 3Depict-$VER.tar.gz


