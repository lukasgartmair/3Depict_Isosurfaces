#!/bin/bash
MSG_FILE=tmp-messages


if [ ! -f configure ] ; then
	echo "Configure not found -- are you running this script from the top dir? " 
	exit 1
fi

#Don't overwrite message file
while [ -f $MSG_FILE ] ; 
do
	MSG_FILE=tmp-$MSG_FILE
done
	

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

#Various release-time checks
#----
#Check version
VER=`ls 3depict-*gz | sed 's/^3depict-\([0-9\.]*\).tar.gz$/\1/' `
echo "Version is apparently :" $VER

if [ x"`grep $VER .hgtags`" == x""  ] ; then
	echo " NOTICE: version number not seen in HG file..." >> $MSG_FILE
else
	echo " NOTICE: Version number exists in HG (OK)" >> $MSG_FILE
fi

#Check version number in various files is set concomitantly.
if [ x"`grep $VER ChangeLog`" == x"" ] ; then
	echo " WARNING: Program version not set to match between configure.ac. and ChangeLog">> $MSG_FILE
fi

if [ x"`grep PROGRAM_VERSION src/basics.cpp | grep $VER`" == x"" ] ; then
	echo " WARNING: Program version not set to match between configure.ac. and basics.cpp">> $MSG_FILE
fi

#Check version number in deb & rpm
if [ x"`head -n 1 packaging/debian/changelog |grep 3depict | grep $VER`"  = x"" ] ; then
	echo " WARNING: Program version does not match between configure.ac and packaging/debian/changelog" >> $MSG_FILE
fi

if [ x"` grep '^Version:' packaging/RPM/3Depict.spec  | grep $VER`"  = x"" ] ; then
	echo " WARNING: Program version does not match between configure.ac and packaging/RPM/3Depict.spec" >> $MSG_FILE
fi

if [ x"` grep 'PRODUCT_VERSION'  packaging/windows-installer/windows-installer.nsi | grep $VER`"  = x"" ] ; then
	echo " WARNING: Program version does not match between configure.ac and packaging/windows-installer/windows-installer.nsi" >> $MSG_FILE
fi

#Check latex manual
if [ x"`grep --after-context=2 'Version:' docs/manual-latex/manual.tex | grep $VER`" == x"" ] ; then
	echo " WARNING: Program version does not match between configure.ac and docs/manual-latex/manual.tex" >> $MSG_FILE
fi

#Check that the fp exceptions are disabled.
if [ x"`grep feenableexcept src/3Depict.cpp | egrep -v '^\s*//'`" != x"" ] ; then
	echo " WARNING: Floating point exceptions still appear to be enabled..." >> $MSG_FILE
fi

#----


rm -rf tarball
mkdir tarball

pushd tarball
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


# Configure stuffs up the directories marked as extra-dist
# if you don't use a trailing slash, some versions of configure die
# if you do, other versions of configure add it as a subdir of itself
# so lets just "fix it"
#--------
pushd 3Depict-$VER/
for i in textures tex-source glade-skeleton 
do
	if [ -d src/$i/$i ] ; then
		mv src/$i/$i/* src/$i/
		rmdir src/$i/$i/
	fi
done

for i in packaging docs m4 translations deps
do
	if [ -d $i/$i ] ; then
		mv $i/$i/* $i/
		rmdir $i/$i/
	fi
done
popd

#make the final source tarball
tar -cz 3Depict-$VER > 3Depict-$VER.tar.gz
popd
#--------


if [ -f $MSG_FILE ] ; then
	echo "------------- SUMMARY ----------"
	cat $MSG_FILE
	echo "--------------------------------"

	rm $MSG_FILE
fi
