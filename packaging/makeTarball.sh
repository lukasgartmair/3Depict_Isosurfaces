#!/bin/bash
MSG_FILE=tmp-messages

NUM_PROCS=4


if [ ! -f configure ] ; then
	echo "Configure not found -- are you running this script from the top dir? " 
	exit 1
fi

#Don't overwrite message file
while [ -f $MSG_FILE ] ; 
do
	MSG_FILE=tmp-$MSG_FILE
done
	

#Try building in debug mode
#-------
make distclean

./configure --disable-debug-checks
if [ $? -ne 0 ] ; then
	echo "no-debug mode failed to configure"
fi

make -j $NUM_PROCS
if [ $? -ne 0 ] ; then
	echo "no-debug mode failed to build"
fi
make distclean
#-------

#Try building in parallel mode
#-------
./configure --enable-openmp-parallel
if [ $? -ne 0 ] ; then
	echo "parallel mode failed to configure"
fi
make -j$NUM_PROCS
if [ $? -ne 0 ] ; then
	echo "parallel mode failed to build"
fi
make distclean
#-------


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
make dist -j $NUM_PROCS

if [ $? -ne 0 ] ; then
	echo "make dist failed";
	exit 1;
fi

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

#check that we are not using preprocessor macros incorrectly
#Apple preprocessor is exactly __APPLE__
RES=`find src/ -name *.cpp -exec grep APPLE {} \; | egrep '^\s*#' | grep -v EFFECTS_WORKAROUND | grep -v __APPLE__`
if [ x"$RES" != x"" ] ; then
	echo " WARNING: possible incorrect APPLE preprocessor token..." >> $MSG_FILE
	echo "$RES" >> $MSG_FILE
fi

RES=`find src/ -name *.h -exec grep APPLE {} \; | egrep '^\s*#' | grep -v EFFECTS_WORKAROUND | grep -v __APPLE__`
if [ x"$RES" != x"" ] ;  then
	echo " WARNING: possible incorrect APPLE preprocessor token..." >> $MSG_FILE
	echo "$RES" >> $MSG_FILE
fi

#Check for editor ~ files and orig files
SOMEFILES=`find ./ -name \*~`
if [ x"$SOMEFILES" != x"" ] ; then
	echo " WARNING : Found some maybe-backup files (~ extension)" >> $MSG_FILE
	echo "$FILES" >> $MSG_FILE
fi

SOMEFILES=`find ./ -name \*.orig`
if [ x"$SOMEFILES" != x"" ] ; then
	echo " WARNING : Found some maybe-backup files (\"orig\" extension)" >> $MSG_FILE
	echo "$FILES" >> $MSG_FILE
fi

SOMEFILES=`find ./ -name \*.rej`
if [ x"$SOMEFILES" != x"" ] ; then
	echo " WARNING : Found some maybe-backup files (\"rej\" extension)" >> $MSG_FILE
	echo "$FILES" >> $MSG_FILE
fi

#Check that PDF manual is built
if [ ! -f docs/manual-latex/manual.pdf ] ; then
	echo " WARNING : PDF manual was not found -- has it been compiled?" >> $MSG_FILE
	echo "$FILES" >> $MSG_FILE
fi

#Check for outstanding mercurial changes
SOME_LINES=`hg diff | wc -l`
if [ x"$SOME_LINES" != x"0" ] ; then
	echo " WARNING : Oustanding mercurial changes! Normally should commit fixes!" >> $MSG_FILE
fi

#Check translation files for "span" element - transifex has a bad habit of encoding whitspace
SPAN_ELEMENT=`cat translations/3Depict_*po | grep "<span"`
if [ x$SPAN_ELEMENT != x"" ] ; then
	echo "WARNING : Found \"span\" element in translation file - normally this is due to transifex incorrectly parsing translation files" >> $MSG_FILE
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

	#copy the manual
	cp ../docs/manual-latex/manual.pdf ./tarball/3Depict-$VER/docs/manual-latex/

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



		for i in packaging docs m4 translations deps test
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
#------

#OK, now try unpacking the thing, then building it.
mkdir tarball/extract/
if [ $? -ne 0 ] ; then
	echo "failed to make tarball extract dir"
fi
pushd tarball/extract

	tar -zxf ../3Depict-$VER.tar.gz

	pushd 3Depict-$VER;
		./configure
		if [ $? -ne 0 ] ; then
			echo "didn't configure after tarball extract"
		fi


		make  -j$NUM_PROCS 
		if [ $? -ne 0 ] ; then
			echo "rebuilding tarball failed."
		else
			echo "tarball rebuild OK!"
		fi

	popd
popd
#--------

if [ -f $MSG_FILE ] ; then
	echo "------------- SUMMARY ----------"
	cat $MSG_FILE
	echo "--------------------------------"

	rm $MSG_FILE
fi
