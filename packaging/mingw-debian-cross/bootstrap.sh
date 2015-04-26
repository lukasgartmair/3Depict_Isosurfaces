#!/bin/bash

#Script to bootstrap 3Depict cross-compilation under debian
# and debian-like systems	
# Note that building wx requires ~8GB of ram (or swap)

#--- Determine which system we wish to build for
#HOST_VAL=x86_64-w64-mingw32 #For mingw64 (windows 64 bit)
#HOST_VAL=i686-w64-mingw32 #For mingw32 (Windows 32 bit)

if [ ! -f host_val ] ; then
	echo "Please select 32 or 64 bit by typing \"32\" or \"64\" (32/64)"
	read HOST_VAL
	
	case $HOST_VAL in
		32)
			HOST_VAL="i686-w64-mingw32"
			;;
		64)
			HOST_VAL="x86_64-w64-mingw32"
			;;
		
		*) 
			echo "Didn't understand HOST_VAL. You can override this by editing the script"
			exit 1
		;;
	esac

	#Save for next run
	echo $HOST_VAL > host_val
else
	HOST_VAL=`cat host_val`
fi


if [ $HOST_VAL != "x86_64-w64-mingw32" ] && [ $HOST_VAL != i686-w64-mingw32 ] ; then
	echo "Unknown HOST_VAL"
	exit 1
else 
	case $HOST_VAL in
		x86_64-w64-mingw32)
			BITS_VAL=64
			;;
		i686-w64-mingw32)
			BITS_VAL=32
			;;
		*)
			echo "Should not have got here - bug!"
			exit 1
			;;
	esac
fi

#----

if [ ! -d code/3Depict ] || [ ! -f code/3Depict/src/3Depict.cpp ] ; then
	echo "3Depict code dir, \"code/3Depict\", appears to be missing. Please place 3Depict source code in this location"
	echo "Aborting"
	exit 1
fi


BASE=`pwd`
PREFIX=/
NUM_PROCS=4

IS_RELEASE=0

if [ `id -u` -eq 0 ]; then
	echo "This script should not be run as root."
	echo " If you know what you are doing, you can disable this check, but be aware you _can_ break your system by doing this."
	exit 1;
fi
#2) own patch for fixing wx-config's lack of sysroot support
PATCHES_WXWIDGETS_PRE="wx_changeset_76890.diff"
PATCHES_WXWIDGETS_POST="wx-config-sysroot.patch"
#1) Zlib no longer needs to explicitly link libc, and will fail if it tries
PATCHES_ZLIB="zlib-no-lc.patch"
#1) Override some configure patches to bypass false positive failures
PATCHES_FTGL="ftgl-disable-doc"
PATCHES_FTGL_POSTCONF="ftgl-override-configure-2"
#1) gettext-tools fails in various places, but we don't actually need it, so turn it off
#2) gettext fails to correctly determine windows function call prefix.
#   should be fixed for gettext > 0.18.1.1 ?
#   https://lists.gnu.org/archive/html/bug-gettext/2012-12/msg00071.html
PATCHES_GETTEXT="gettext-disable-tools gettext-fix-configure-versions"    #gettext-win32-prefix

PATCHES_GLEW="glew-makefile.base"

PATCHES_MATHGL="mathgl-openmp-linker-flag"
PATCH_LIST="$PATCHES_WXWIDGETS_PRE $PATCHES_WXWIDGETS_POST $PATCHES_GSL $PATCHES_ZLIB $PATCHES_LIBPNG $PATCHES_GETTEXT $PATCHES_FTGL $PATCHES_GLEW $PATCHES_MATHGL $PATCHES_FTGL_POSTCONF"

BUILD_STATUS_FILE="$BASE/build-status"
PATCH_STATUS_FILE="$BASE/patch-status"
PATCH_LEVEL=1

function isBuilt()
{
	if [ x`cat ${BUILD_STATUS_FILE} | grep $ISBUILT_ARG` != x"" ]  ; then
		ISBUILT=1
	else
		ISBUILT=0
	fi
}

function applyPatches()
{
	for i in $APPLY_PATCH_ARG
	do
		if [ x"`cat $PATCH_STATUS_FILE | grep -x "$i"`" != x"" ] ; then
			echo "Patch already applied :" $i
			continue
		fi

		echo "Applying patch:" $i
		patch -tN -p$PATCH_LEVEL  < $BASE/patches/$i
		
		if [ $? -ne 0 ] ; then
			echo "Failed applying patch :" $i
			exit 1
		fi
		echo "applied patch"
		echo $i >> $PATCH_STATUS_FILE
	done
}

function install_mingw()
{

	echo "Checking mingw install"
	#install mingw and libtool (we will need it...)

	GET_PACKAGES="";
	for i in $MINGW_PACKAGES
	do
		if [ x`dpkg --get-selections | grep ^$i | awk '{print $1}'  ` != x"$i" ] ; then
			GET_PACKAGES="$GET_PACKAGES $i";
		fi
	done

	if [ x"$GET_PACKAGES" != x"" ] ; then
		echo "Requesting install of mingw :" $GET_PACKAGES
		sudo apt-get install $GET_PACKAGES libtool || { echo "Mingw install failed";  exit 1 ; }
	fi

}

function grabDeps()
{
	pushd deps 2>/dev/null

	DEB_PACKAGES="expat freetype ftgl gettext gsl libpng libxml2 mathgl qhull tiff wxwidgets3.0 zlib glew libvigraimpex"
	if [ x$DIST_NAME == x"Ubuntu" ] || [ x$DIST_NAME == x"LinuxMint" ]  ; then 
       		LIBJPEGNAME="libjpeg6b"
	else
		#Libjpeg seems to be forked/renamed very frequently in debian
		# Likely a new libjpeg will need to be picked each time this script is run
		LIBJPEGNAME="libjpeg-turbo"
	fi
	DEB_PACKAGES="$DEB_PACKAGES $LIBJPEGNAME"

	GET_PACKAGES=""
	for i in $DEB_PACKAGES
	do
		FNAME=`ls packages/${i}_*.orig.* 2> /dev/null`
		#If filename is empty, we will need to retreive it from
		# interwebs
		if [ x"$FNAME" == x"" ] ; then
			GET_PACKAGES="${GET_PACKAGES} $i"
		fi
	done

	#grab packages if they are not already on-disk
	if [ x"$GET_PACKAGES" != x"" ] ; then
		apt-get source $GET_PACKAGES

		if [ $? -ne 0 ] ; then
			echo "Package retrieval failed"
			echo "apt-get source failed... Maybe check internet connection, then try updating package database, then re-run?"
			echo " other possibilities could include, eg, that the required package is not available in the debian archive.."
			exit 1
		fi

		#Strip patches from the build and patch status files, 
		# if we are retriving new packages
		for i in $GET_PACKAGES
		do
			grep -v $i ../build-status > tmp
			mv tmp ../build-status
			
			grep -v $i ../patch-status > tmp
			mv tmp ../patch-status


		done
	fi


	#Move debian stuff into packages folder
	if [ x"$GET_PACKAGES" != x"" ] ; then
		mv *.orig.* *.debian.* *.dsc *.diff.* packages 
	fi

	#Check that we have untarred all the required packages
	# (eg we downloaded one, and wiped it at some stage)
	# if not, pull the package out, and re-build it
	GET_PACKAGES=""
	for i in $DEB_PACKAGES
	do
		#if we have a package file (dsc), and no folder, add it to the list of packages
		FNAME=`ls packages/$i*dsc 2> /dev/null` 
		DNAME=`ls -ld $i* | grep ^d | awk '{print $NF}'`
		if [ x"$FNAME" != x"" ] && [ x"$DNAME" == x""  ] ; then
			GET_PACKAGES="${GET_PACKAGES} $i"
		fi
	done
	
	#Unpack pre-existing package
	for i in $GET_PACKAGES
	do
		mv packages/$i*.* . || { echo "existing package extraction failed "; exit 1; } 
		dpkg-source -x $i*dsc
		#move package back
		mv ${i}_*.* packages/
		
		#wipe record of any patches for this package
		grep -v $i ../patch-status  > tmp
		mv tmp ../patch-status
		
		#wipe record of build
		grep -v $i ../build-status  > tmp
		mv tmp ../build-status
	done

	#extract libiconv if needed
	#--
	LIBICONV=libiconv-1.14
	if [ ! -f packages/${LIBICONV}.tar.gz ] ; then
		wget "http://ftp.gnu.org/gnu/libiconv/libiconv-1.14.tar.gz" || { echo "Libiconv download failed "; exit 1; }
		mv ${LIBICONV}.tar.gz packages/
	fi

	#Check for SHA1 goodness
	if [ x`sha1sum packages/${LIBICONV}.tar.gz | awk '{print $1}'` != x"be7d67e50d72ff067b2c0291311bc283add36965" ] ; then
		echo "SHA1 sum mismatch for libiconv"
		exit 1
	fi
	
	#Extract
	if [ ! -d ${LIBICONV} ] ; then
		tar -zxf packages/${LIBICONV}.tar.gz || { echo "failed extracting iconv "; exit 1; }
	fi	

	#---

	#We also need to install nsis, though it is not a strict "dependency" per-se.
	if [ x`which makensis` == x"" ] ; then
		echo "Installing nsis via apt-get";
		sudo apt-get install nsis || { echo "Failed installation"; exit 1; }
	fi

	popd 2> /dev/null

}

function createBaseBinaries()
{
	pushd bin >/dev/null

	#Create symlinks to the compiler, which will be in our
	# primary path, and thus override the normal compiler
	
	#use ccache if it is around
	if [  x`which ccache` != x"" ] ; then
		echo -n "Enabling ccache support..."
		USE_CCACHE=1
	else
		echo -n "Making direct compiler symlinks..."
		USE_CCACHE=0
	fi

	for i in g++ cpp gcc c++
	do
		#Skip existing links
		if [ -f ${HOST_VAL}-$i ] || [ -L ${HOST_VAL}-$i ] ; then
			continue;
		fi

		if [ $USE_CCACHE == 1 ] ; then

			ln -s `which ccache` ${HOST_VAL}-$i
		else

			ln -s `which  ${HOST_VAL}-$i`
			
		fi
		
		if [ $? -ne 0 ] ; then
			echo "Failed when making compiler symlinks"
			exit 1
		fi
	done

	echo "done"
	popd >/dev/null
}

function fix_la_file
{
	#Need to fix .la files which have wrong path
	# due to libtool not liking cross-compile
	pushd lib/ >/dev/null
	sed -i 's@//lib@'${BASE}/lib@ ${FIX_LA_FILE_ARG}*.la
	sed -i 's@/lib/lib/@/lib/@'  ${FIX_LA_FILE_ARG}*.la
	popd >/dev/null
}

function build_zlib()
{
	ISBUILT_ARG="zlib"
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi

	pushd deps >/dev/null
	pushd zlib-* >/dev/null


	if [ $? -ne 0 ] ; then
		echo "Zlib dir missing, or duplicated?"
		exit 1
	fi
	
	make clean
	rm -f configure.log

	
	./configure  --prefix="/" || { echo "ZLib configure failed"; exit 1; } 
	
	APPLY_PATCH_ARG="$PATCHES_ZLIB"
	applyPatches

	make -j $NUM_PROCS || { echo "ZLib build failed"; exit 1; } 

	make install DESTDIR="$BASE"|| { echo "ZLib install failed"; exit 1; } 

	#Move the .so to a .dll
	mv ${BASE}/lib/libz.so ${BASE}/lib/libz.dll

	#Remove the static zlib. We don't want it
	rm $BASE/lib/libz.a

	popd >/dev/null
	popd >/dev/null

	echo "zlib" >> $BUILD_STATUS_FILE
}

function build_glew()
{
	ISBUILT_ARG="glew"
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi

	#Perform dynamic modification of patch
	if [ x"`grep HOST_VAL patches/glew-makefile.base`" == x""  -o   x"`grep BASEDIR patches/glew-makefile.base`" == x"" ] ; then
		echo "patches/glew-makefile did not contain replacement keywords"
		exit 1
	fi

	#Modify the patch appropriately
	cp patches/glew-makefile.base patches/glew-makefile
	sed -i "s@HOST_VAL@$HOST_VAL@" patches/glew-makefile
	sed -i "s@BASEDIR@$BASE@" patches/glew-makefile
	
	pushd deps >/dev/null
	pushd glew-* >/dev/null


	if [ $? -ne 0 ] ; then
		echo "glew dir missing, or duplicated?"
		exit 1
	fi
	

	APPLY_PATCH_ARG="glew-makefile"
	applyPatches

	make clean
	rm -f configure.log
	
	LD=$CC make -j $NUM_PROCS || { echo "glew build failed"; exit 1; } 

	make install DESTDIR="$BASE"|| { echo "glew install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null

	#remove static library, as this can be incorrectly caught by linker
	# leading to confusing messages
	rm ${BASE}/lib/libglew*.a

	echo "glew" >> $BUILD_STATUS_FILE
}

function build_libpng()
{
	ISBUILT_ARG="libpng"
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi
	pushd deps >/dev/null
	pushd libpng-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "libpng dir missing, or duplicated?"
		exit 1
	fi

	make clean

	./configure --host=$HOST_VAL --enable-shared --disable-static || { echo "Libpng configure failed"; exit 1; } 

	#Hack to strip linker version script
	# eg as discussed : 
	# https://github.com/Pivosgroup/buildroot-linux/issues/2
	# The error is not 100% clear. if the build script has 
	# access to standard debian -I paths, then it goes away
	# version script documentation is sparse to non-existant.
	# the only really useful thing i've found is this:
	# http://www.akkadia.org/drepper/dsohowto.pdf
	sed -i 's/.*version-script.*//' Makefile Makefile.am

	make -j $NUM_PROCS  || { echo "libpng build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "libpng install failed"; exit 1; } 

	#The new libpng shoudl install (when it works, which it does not for us)
	# the headers in a new subfolder, which is not compatible with most buildsystems
	# so make some symlinks
	#---
	# First simulate libpng's installer working (TODO: Why is this not working? It did before..)
	mkdir -p ../../include/libpng12/ 
	cp -p png.h pngconf.h ../../include/libpng12/
	# Then actually make the symlinks
	pushd ../../include/ > /dev/null
	ln -s libpng12/pngconf.h
	ln -s libpng12/png.h
	popd >/dev/null

	#Aaand it doesn't install the libraries, so do that too.
	cp -p .libs/*.dll $BASE/lib/
	cp -p .libs/*.la $BASE/lib/
	#----


	#Remove superseded libpng3
	rm ${BASE}/lib/libpng-3*
	ln -s ${BASE}/lib/libpng12-0.dll  ${BASE}/lib/libpng.dll

	popd >/dev/null
	popd >/dev/null

	FIX_LA_FILE_ARG=libpng
	fix_la_file
	
	echo "libpng" >> $BUILD_STATUS_FILE
}

function build_libxml2()
{
	NAME="libxml2"
	ISBUILT_ARG=${NAME}
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi

	pushd deps >/dev/null
	LIBXMLVER=`ls -d libxml2-* | sed 's/libxml2-//'`
	pushd libxml2-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "libxml2 dir missing, or duplicated?"
		exit 1
	fi

	#Libxml 2.8 and up doesn't need patching.
	# note that --compare-versions returns 0 on truth, and 1 on false
	dpkg --compare-versions  $LIBXMLVER lt 2.8
	if [ $? -eq 0 ] ; then
		echo "WARNING : Previous attempts to use libxml2 < 2.8 have failed."
		echo " it is recommended to manually obtain the .dsc,.orig.tar.gz and .debian.tar.gz from"
		echo " http://packages.debian.org/source/wheezy/libxml2 . download these, then replace the .dsc,.orig.tar.gz and .debian.tar.gz in deps/packages/"
		exit 1
	fi

	make clean

	#Modifications
	#	Disable python, because sys/select.h is not in mingw
	./configure --host=$HOST_VAL --without-python --without-html --without-http --without-ftp --without-push --without-writer --without-push --without-legacy --without-xpath --without-iconv  --enable-shared=yes --enable-static=no --prefix=/ || { echo "Libxml2 configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "libxml2 build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "libxml2 install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null

	FIX_LA_FILE_ARG=libxml2
	fix_la_file

	#For compat, link libxml2/libxml to libxml/
	pushd include >/dev/null
	ln -s libxml2/libxml libxml
	popd >/dev/null

	#The dll gets installed into bin/, link it to lib/
	pushd lib > /dev/null
	ln -s ../bin/libxml2-[0-9]*.dll 
	popd > /dev/null


	echo ${NAME} >> $BUILD_STATUS_FILE
}

function build_libjpeg()
{
		
	NAME=$LIBJPEGNAME
	ISBUILT_ARG=${NAME}
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi
	pushd deps >/dev/null
	pushd ${NAME}*[0-9]* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "${NAME} dir missing, or duplicated?"
		exit 1
	fi

	make clean

	./configure --host=$HOST_VAL --enable-shared --disable-static --prefix=/ || { echo "$NAME configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "$NAME build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "$NAME install failed"; exit 1; } 
	
	#DLL needs to be copied into lib manually
	cp -p .libs/${NAME}-[0-9]*.dll $BASE/lib/ 

	popd >/dev/null
	popd >/dev/null

	FIX_LA_FILE_ARG=$NAME
	fix_la_file
	echo ${NAME}  >> $BUILD_STATUS_FILE
}

function build_libtiff()
{
	NAME="libtiff"
	ISBUILT_ARG=${NAME}
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi
	pushd deps >/dev/null
	pushd tiff*[0-9]* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "libtiff dir missing, or duplicated?"
		exit 1
	fi
	./configure --host=$HOST_VAL --enable-shared --disable-static --prefix=/ || { echo "Libtiff configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "libtiff build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "libtiff install failed"; exit 1; } 
	
	#DLL needs to be copied into lib manually
	cp -p .libs/${NAME}-[0-9]*.dll $BASE/lib/ 

	popd >/dev/null
	popd >/dev/null
	
	FIX_LA_FILE_ARG=libtiff
	fix_la_file
	echo ${NAME} >> $BUILD_STATUS_FILE
}

function build_qhull()
{
	NAME="libqhull"
	ISBUILT_ARG=${NAME}
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi
	
	pushd deps >/dev/null
	pushd qhull-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "qhull dir missing, or duplicated?"
		exit 1
	fi

	make clean

	sed -i "s/ gcc$/${HOST_VAL}-gcc/" Makefile
	sed -i "s/ g++$/${HOST_VAL}-g++/" Makefile

	make SO="dll" -j $NUM_PROCS 
	find ./ -name \*dll -exec cp {} ${BASE}/bin/	
	make SO="dll" -j $NUM_PROCS || { echo "qhull build failed"; exit 1; } 
	make install DESTDIR="$BASE"|| { echo "qhull install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null

	ln -s ${BASE}/include/libqhull ${BASE}/include/qhull
	
	FIX_LA_FILE_ARG=libqhull
	fix_la_file
	echo ${NAME} >> $BUILD_STATUS_FILE
}


function build_expat()
{
	NAME="libexpat"
	ISBUILT_ARG=${NAME}
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi
	pushd deps >/dev/null
	pushd expat-** >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "expat dir missing, or duplicated?"
		exit 1
	fi

	./configure --host=$HOST_VAL --enable-shared --disable-static --prefix=/ || { echo "$NAME configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "$NAME build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "$NAME install failed"; exit 1; } 

	#DLL needs to be copied into lib manually
	cp -p .libs/${NAME}-[0-9]*.dll $BASE/lib/ 

	popd >/dev/null
	popd >/dev/null



	FIX_LA_FILE_ARG=libexpat
	fix_la_file
	echo ${NAME} >> $BUILD_STATUS_FILE
}

function build_gsl()
{
	NAME="libgsl"
	ISBUILT_ARG=${NAME}
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi
	
	pushd deps >/dev/null
	pushd gsl-* >/dev/null
		
	if [ $? -ne 0 ] ; then
		echo "gsl dir missing, or duplicated?"
		exit 1
	fi

	make clean

	mkdir -p doc
	echo "all:" > doc/Makefile.in 
	echo "install:" >> doc/Makefile.in
	echo "clean:" >> doc/Makefile.in

	APPLY_PATCH_ARG=$PATCHES_GSL
	applyPatches

	./configure --host=$HOST_VAL --enable-shared --disable-static --prefix=/ || { echo "gsl configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "gsl build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "gsl install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null


	FIX_LA_FILE_ARG=libgsl
	fix_la_file
	echo ${NAME} >> $BUILD_STATUS_FILE
}

function build_wx()
{
	NAME="libwx"
	ISBUILT_ARG=${NAME}
	isBuilt

	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi
	
	pushd deps >/dev/null
	pushd wxwidgets[23].[0-9]-* >/dev/null
       	
	if [ $? -ne 0 ] ; then
		echo "wxwidgets dir missing, or duplicated?"
		exit 1
	fi

	make clean

	APPLY_PATCH_ARG=$PATCHES_WXWIDGETS_PRE
	applyPatches
	WX_DISABLE="--disable-compat26 --disable-compat28 --disable-ole --disable-dataobj --disable-ipc --disable-apple_ieee --disable-zipstream --disable-protocol_ftp --disable-mshtmlhelp --disable-aui --disable-mdi --disable-postscript --disable-datepick --disable-splash --disable-wizarddlg --disable-joystick --disable-loggui --disable-debug --disable-logwin --disable-logdlg --disable-tarstream --disable-fs_archive --disable-fs_inet --disable-fs_zip --disable-snglinst --disable-sound --disable-variant --without-regex"

	./configure --host=$HOST_VAL --enable-shared --disable-static --with-opengl --enable-unicode --without-regex --prefix=/ || { echo "wxwidgets configure failed"; exit 1; } 

	#TODO: Where is this coming from ???
	for i in `find ./ -name Makefile | grep -v samples | grep -v wxPython`
	do
		sed -i "s@-luuid-L@ -luuid -L@" $i
	done	
	
	make -j $NUM_PROCS || { echo "wxwidgets build failed"; exit 1; } 
	make install DESTDIR="$BASE"|| { echo "wxwidgets install failed"; exit 1; } 


	
	popd >/dev/null
	popd >/dev/null

	pushd ./bin/
	unlink wx-config


	#Search for the wx-config file. It get installed into /lib/wx, but has
	# unusual naming conventions
	WX_CONFIG_FILE=`find ${BASE}/lib/wx/config/ -type f -executable -name \*release-\*`
	if [ x$WX_CONFIG_FILE == x"" ] ; then
		WX_CONFIG_FILE=`find ${BASE}/lib/wx/config/ -type f  -executable -name \*-unicode-\*`
	fi

	if [ x$WX_CONFIG_FILE == x"" ] ; then
		echo "Couldn't find the wx-config script."
		exit 1
	fi

	cp $WX_CONFIG_FILE wx-config 
	APPLY_PATCH_ARG=$PATCHES_WXWIDGETS_POST
	PATCH_LEVEL=0
	applyPatches
	PATCH_LEVEL=1
	sed -i "s@REPLACE_BASENAME@${BASE}@" wx-config || { echo "Failed to update wx-config with build root,. Aborting";  exit 1; }
	popd

	pushd ./lib/
	ln -s wx-3.0/wx/ wx
	popd


	echo ${NAME} >> $BUILD_STATUS_FILE

}

function build_freetype()
{
	NAME="libfreetype"
	ISBUILT_ARG=${NAME}
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi
	pushd deps >/dev/null
	pushd freetype-* >/dev/null
		
	if [ $? -ne 0 ] ; then
		echo "freetype dir missing, or duplicated?"
		exit 1
	fi

	tar -xjf freetype-[0-9]*bz2  || { echo "freetype decompress failed" ; exit 1; } 

	pushd freetype-[0-9]*
	make clean
	./configure --host=$HOST_VAL --enable-shared --disable-static --without-png --prefix=/ || { echo "freetype configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "freetype build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "freetype install failed"; exit 1; } 

	popd >/dev/null
	
	popd >/dev/null
	popd >/dev/null
	
	
	FIX_LA_FILE_ARG=libfreetype
	fix_la_file

	#freetype has moved headers. Add symlink for backwards compat

	pushd include >/dev/null
	ln -s freetype2/freetype/
	popd >/dev/null
	
	echo ${NAME} >> $BUILD_STATUS_FILE
}


function build_libiconv()
{
	NAME="iconv"
	ISBUILT_ARG=${NAME}
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi

	pushd deps >/dev/null
	pushd libiconv-* >/dev/null
		
	if [ $? -ne 0 ] ; then
		echo "libiconv dir missing, or duplicated?"
		exit 1
	fi

	make clean
	./configure --host=$HOST_VAL --enable-shared --disable-static --prefix=/ || { echo "libiconv configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "libiconv build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "libiconv install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null
	
	FIX_LA_FILE_ARG=libiconv
	fix_la_file
	echo ${NAME} >> $BUILD_STATUS_FILE
}

function build_gettext()
{
	NAME="gettext"
	ISBUILT_ARG=${NAME}
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi
	pushd deps >/dev/null
	pushd gettext-* >/dev/null
		
	if [ $? -ne 0 ] ; then
		echo "$NAME dir missing, or duplicated?"
		exit 1
	fi

	make clean

	APPLY_PATCH_ARG=$PATCHES_GETTEXT
	applyPatches
	automake

	./configure --host=$HOST_VAL --disable-threads --enable-shared --disable-static --prefix=/ || { echo "$NAME configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "$NAME build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "$NAME install failed"; exit 1; } 

	#FIXME: I had to copy the .lib, .la and .a files manually
	# I don't know why the makefile does not do this.
	cp gettext-runtime/intl/.libs/libintl.{la,lib,a} ${BASE}/lib/ || {  echo "semi-manual copy of libintl failed"; exit 1; } 
	
	popd >/dev/null
	popd >/dev/null
	
	FIX_LA_FILE_ARG=libintl
	fix_la_file
	FIX_LA_FILE_ARG=libcharset
	fix_la_file
	
	echo ${NAME} >> $BUILD_STATUS_FILE
}

function build_mathgl()
{
	NAME="libmgl"
	ISBUILT_ARG=${NAME}
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi
	pushd deps >/dev/null
	pushd mathgl-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "mathgl dir missing, or duplicated?"
		exit 1
	fi
	make clean

	APPLY_PATCH_ARG=$PATCHES_MATHGL
	applyPatches

	if [ -d $BASEDIR/include/mgl2 ] ; then
		echo "there are mgl2 headers already installed. Abort abort!"\
		exit 1
	fi

	LIBS=-lpng cmake -Denable-gsl="yes" -Denable-mpi="no"  -DCMAKE_INSTALL_PREFIX="$BASE" -DCMAKE_TOOLCHAIN_FILE=../../patches/cmake-toolchain$BITS_VAL -DPNG_PNG_INCLUDE_DIR=${BASEDIR}/include/

	make -j $NUM_PROCS

	if [ x"`find ./ -name \*dll`" == x"" ] ; then
		echo "Did cmake fail to make a DLL? Cmake rarely builds cleanly, but I should be able to find the DLL..."
		exit 1
	fi
		
	make install
	
	ln -s ${BASE}/include/mgl2 ${BASE}/include/mgl

	popd >/dev/null
	popd >/dev/null
	FIX_LA_FILE_ARG=libmgl
	fix_la_file
	
	echo ${NAME} >> $BUILD_STATUS_FILE
}

function build_libvigra()
{
	NAME="libvigra"
	ISBUILT_ARG=${NAME}
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi
	pushd deps >/dev/null
	pushd libvigraimpex-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "$NAME dir missing, or duplicated?"
		exit 1
	fi
	make clean

	APPLY_PATCH_ARG=$PATCHES_MATHGL
	applyPatches

	cmake -DCMAKE_INSTALL_PREFIX="$BASE" -DCMAKE_TOOLCHAIN_FILE=../../patches/cmake-toolchain$BITS_VAL -DPNG_PNG_INCLUDE_DIR=${BASEDIR}/include/

	make -j $NUM_PROCS
	
	make install || { echo "$NAME install failed"; exit 1; } 


	popd >/dev/null
	popd >/dev/null
	FIX_LA_FILE_ARG=libmgl
	fix_la_file
	
	echo ${NAME} >> $BUILD_STATUS_FILE
}

function build_ftgl()
{
	NAME="libftgl"
	ISBUILT_ARG=${NAME}
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi
	pushd deps >/dev/null
	pushd ftgl-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "ftgl dir missing, or duplicated?"
		exit 1
	fi
	make clean

	APPLY_PATCH_ARG="$PATCHES_FTGL"
	applyPatches

	autoreconf
	libtoolize

	APPLY_PATCH_ARG="$PATCHES_FTGL_POSTCONF"
	applyPatches
	#rename GLU to glu32 and gl to opengl32
	sed -i s/'\-lGLU'/-lglu32/ configure
	sed -i s/'\-lGL'/-lopengl32/ configure

	#configure tries to link against wrong prototypes. Override this
	sed -i 's/char glBegin() ;//' configure
	sed -i 's/char gluNewTess ();//' configure

	sed -i 's/return glBegin ()/return 0;/' configure
	sed -i 's/return gluNewTess ()/return 0;/' configure
	sed -i 's/return glBegin(GL_POINTS)/return 0;/' configure



	./configure --host=$HOST_VAL --enable-shared --disable-static --prefix=/ || { echo "ftgl configure failed"; exit 1; } 


	#MAkefile refers to ECHO variable for reporting completion, which does not exist
	sed -i 's/ECHO_C =/ECHO=echo/' Makefile
	sed -i "s@-I//@-I${BASE}/@" Makefile
	sed -i 's/ECHO_C =/ECHO=echo/' Makefile
	
	#HACK - find all -I// and -L// and replace them with something sane
	find ./ -name Makefile -exec sed -i "s@-I//@-I${BASE}/@" {} \;
	find ./ -name Makefile -exec sed -i "s@-L//@-L${BASE}/@" {} \;

	make -j $NUM_PROCS || { echo "ftgl build failed"; exit 1; } 
	
	DESTDIR="$BASE" make install | { echo "ftgl install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null
	FIX_LA_FILE_ARG=libftgl
	fix_la_file
	
	echo ${NAME} >> $BUILD_STATUS_FILE
}

function createDirLayout()
{
	NEEDED_DIRS="bin lib include deps/packages"
	for i in $NEEDED_DIRS
	do
		if [ ! -d $i ] ; then
			mkdir -p $i;
		fi
	done
}

function checkPatchesExist()
{	
	echo -n "checking patches exist..."
	if [ x"$PATCH_LIST" == x"" ] ; then
		return;
	fi

	if [ ! -d patches ] ; then
		echo "Patch directory does not exist, but patches are needed"
		echo "Need : " $PATCH_LIST
		exit 1
	fi

	for i in $PATCH_LIST
	do
		if [ ! -f "patches/$i" ]  ; then
			echo "patches/$i" does not exist
			exit 1;
		fi
	done
	echo "done"
}

#Ensure we are running under  a whitelisted host
function checkHost()
{
	echo -n "Host checks... "
	if [ x`which uname` == x"" ] ; then
		echo "Uname missing... Shouldn't be... Can't determine arch - bailing out."
		exit 1
	fi

	if [ x`uname -s` != x"Linux" ] ; then
		echo "This is only meant to run under linux.. and probably only debian-like systems."
		exit 1
	fi


	HOSTTYPE=`uname -m`

	if [ x"$HOSTTYPE" != x"x86_64" ] ; then
		echo "This only works on x86_64 builder machines - you'll need to modify the script to handle other arches. Sorry."
		exit 1
	fi

	BINARIES_NEEDED="apt-get sudo patch grep sed awk"
	
	for i in $BINARIES_NEEDED 
	do
		if [ x`which $i` == x"" ] ; then
			echo "Missing binary : $i  - aborting"
			exit 1
		fi
	done
	
	if [ x`which lsb_release` != x"" ] ; then
		#Possible results
		# Debian, LinuxMint,Ubuntu,(others)
		DIST_NAME=`lsb_release -i | awk -F: '{print $2}' | sed 's/\s//g'`
		if [ x$DIST_NAME != x"Debian" ] ; then
			echo "This is meant to target debian. I'm not sure if it will work on your system. You'll need to work that out."
			echo "Sleeping for 4 seconds."
			sleep 4
		fi
	fi
	
	echo "done"
}

#Build 3Depict
function build_3Depict()
{
	pushd code/3Depict
	make distclean


	CONF_FLAG="--host=$HOST_VAL --with-libqhull-link=-lqhull_p"
	if [ $IS_RELEASE -ne 0 ] ; then
		CONF_FLAG="$CONF_FLAG --disable-debug-checks --enable-openmp-parallel"
	fi

	FTGL_CFLAGS="-I${BASE}/include/freetype/" CFLAGS="$CFLAGS -DUNICODE" CPPFLAGS="${CPPFLAGS} -DUNICODE" ./configure  $CONF_FLAG

	if [ $? -ne 0 ] ; then
		echo "Failed 3Depict configure"
		exit 1
	fi

	#sanity check that windres is activated
	if [ x`grep HAVE_WINDRES_TRUE config.log | grep '#' ` != x"" ] ; then
		echo "Windres appears to be commented out. Shouldn't be for windows builds"
		exit 1
	fi

	#Check that wx's manifest matches our arch
	MANIFEST=`find ../../include/ -name wx.manifest`
	if [ x"$MANIFEST" == x"" ] ; then
		echo "Didnt' find manifest!"
		exit 1
	fi
	case $BITS_VAL in
		32)
			MANIFEST_TARG=x86
			MANIFEST_NOT=amd64
			;;
		64)
			MANIFEST_TARG=amd64
			MANIFEST_NOT=x86
			;;
	esac

	if [  x"`grep -i $MANIFEST_TARG $MANIFEST`" == x"" ] ; then
		echo "Manifest arch does not match!"	
		echo " file examined: $MANIFEST"
		echo " Expected :" $MANIFEST_TARG 
		exit 1
	fi

	if [  x"`grep -i $MANIFEST_NOT $MANIFEST`" != x"" ] ; then
		echo "Manifest arch does not match!"	
		echo " file examined: $MANIFEST"
		echo " This should be missing, but isnt:" $MANIFEST_NOT 
		exit 1
	fi

       #HACK - strip all makefiles of -D_GLIBCXX_DEBUG
       #	mingw & GLIBCXX_DEBG don't play nice
	find ./ -name Makefile -exec sed -i 's/-D_GLIBCXX_DEBUG//g' {} \;
	#HACK - find all -I// and -L// and replace them with something sane
	find ./ -name Makefile -exec sed -i "s@-I//@-I${BASE}/@" {} \;
	find ./ -name Makefile -exec sed -i "s@-L//@-L${BASE}/@" {} \;
	
	make -j$NUM_PROCS
	if [ $? -ne 0 ] ; then
		echo "Failed 3Depict build"
		exit 1
	fi

	#if the locales are missing, try to rebuild them
	if [ x`find locales/ -name \*.mo` = x""  ] ; then
		
		if [ x`which msgmerge` == x"" ] ; then
			echo "Translations do not appear to be built for 3depict. Need to install translation builder, gettext."
			sudo apt-get install gettext
		fi
		pushd translations
		./makeTranslations || { echo "Translation build failed."; exit 1; }

		mkdir -p ../locales/
		for i in *.mo
		do
			TARGDIR=../locales/`echo $i | sed ' s/\.mo//' | sed 's/3Depict_//'`  
			mkdir -p $TARGDIR
			cp $i $TARGDIR/
		done
		popd
	fi

	popd > /dev/null
}

#Build the nsis package
function make_package()
{
	pushd ./code/3Depict 2> /dev/null

	#Check that the PDF manual has been built
	if [ ! -f docs/manual-latex/manual.pdf ] ; then
		echo "PDF Manual not built. Building"
	
		pushd docs/manual-latex
		pdflatex manual.tex && bibtex manual && pdflatex manual.tex  || { echo " Manual not pre-built, and failed to build. Aborting" ; exit 1; }
		popd
		if [ ! -f docs/manual-latex/manual.pdf ] ; then 
			echo "Failed to build manual, even though latex completed with no errors. Aborting " 
			exit 1;
		fi
	fi


	NSI_FILE=./windows-installer.nsi

	#copy as needed
	# Due to debian bug : #704828, makensis cannot correctly handle symlinks,
	# so don't use symlinks
	if [ ! -f `basename $NSI_FILE`  ] ; then
		cp ./packaging/mingw-debian-cross/windows-installer.nsi .
	fi

	
	if [ ! -f $NSI_FILE ] ; then
		echo "NSI file missing whilst trying to build package"
		exit 1;
	fi

	#Check NSI file has PROGRAMFILES / PROGRAMFILES64 set
	if [ x"`grep PROGRAMFILES64 $NSI_FILE`" == x"" -a $BITS_VAL == 64 ] ; then
		echo "NSI file should contain PROGRAMFILES64 output path."
		exit 1;
	else
		if [ x"`grep PROGRAMFILES64 $NSI_FILE`" != x"" -a $BITS_VAL == 32 ] ; then
			echo "NSI file contained 64 bit install dir, but this is 32"
			exit 1;
		fi
	fi
	
	
	 

	echo -n " Copying dll files... "
	SYSTEM_DLLS="(ADVAPI32.dll|COMCTL32.DLL|COMDLG32.DLL|GDI32.dll|KERNEL32.dll|ole32.dll|OLEAUT32.dll|RPCRT4.dll|SHELL32.DLL|USER32.dll|WINMM.DLL|WINSPOOL.DRV|WSOCK32.DLL|GLU32.dll|OPENGL32.dll|msvcrt.dll|WS2_32.dll)"

	DLL_FILES=`${HOST_VAL}-objdump -x src/3Depict.exe | grep 'DLL Name:' | awk '{print $3}' | egrep -i -v ${SYSTEM_DLLS}`
	FOUND_DLLS=""
	SYS_DIR=/usr/lib/gcc/${HOST_VAL}/

	echo "DEBUG :" $DLL_FILES
	rm -f tmp-dlls tmp-found-dlls

	#copy the DLL files from system or 
	# from the build locations
	while [ x"$DLL_FILES" != x"" ] ;
	do
		echo $DLL_FILES | tr '\ ' '\n' > tmp-dlls
		DLL_FILES=`cat tmp-dlls | sort | uniq`

		echo "Looking for these:" $DLL_FILES
		echo "Have found these:" $FOUND_DLLS
		for i in $DLL_FILES
		do
			HAVE_DLL=0
			for j in ${BASE}/lib/ ${BASE}/bin/ $SYS_DIR /usr/${HOST_VAL}/lib/
			do
				FIND_RES=`find $j -name $i | head -n 1`
				if [ x$FIND_RES != x"" ] ; then
					HAVE_DLL=1;
					cp $FIND_RES ./src/
					FOUND_DLLS="$FOUND_DLLS `basename $FIND_RES`"
					break;
				fi
			done

			if [ $HAVE_DLL -eq 0 ] ; then
				echo "Couldnt find DLL :" 
				echo " $i "
				echo " looked in ${BASE}/lib/ and $SYS_DIR"
				exit 1;
			fi
		done

		#Update the list of dll files
		echo $FOUND_DLLS |tr '\ ' '\n' | sort | uniq > tmp-found-dlls
		DLL_FILES=`${HOST_VAL}-objdump -x ./src/3Depict.exe ./src/*dll | grep 'DLL Name:' | awk '{print $3}' | egrep -i -v ${SYSTEM_DLLS} | grep -iv -f tmp-found-dlls | sort | uniq`

	done
	echo "done."

	if [ $IS_RELEASE -ne 0 ] ; then
		#Strip debugging information
		pushd src/ > /dev/null
		strip *.dll *.exe
		popd > /dev/null
	fi

	#Check for differeent DLL types (eg 32/64)
	DLL_FILE_OUT="";
	for i in $FOUND_DLLS
	do
		J=`file $i | awk -F: '{print $2}' | sed 's/\s*//'`
		if [ x"$DLL_FILE_OUT" == x"" ] ; then	
			DLL_FILE_OUT=$j
		else
			if [ x"$DLL_FILE_OUT" != x"$j" ] ; then
				echo "DLL Mismatched file info. $i"
				exit 1
			fi
		fi
	done

	if [ x"`cat windows-installer.nsi | grep INSERT_DLLS_HERE`" == x"" ]  ||  [ x"`cat windows-installer.nsi | grep INSERT_UNINST_DLLS_HERE`" == x"" ] ; then
		echo "DLL insertion/removal tokens not found. Was looking for INSERT_DLLS_HERE and INSERT_UNINST_DLLS_HERE"
		exit 1
	fi

	#Insert DLL names automatically
	cp windows-installer.nsi tmp.nsi
	echo $FOUND_DLLS | sed 's/ /\n/g' |  sed 's@^@  File \"src\\@' | sed 's/$/\"/' > tmp-insert
	perl -ne 's/^  ;INSERT_DLLS_HERE/`cat tmp-insert$1`/e;print' tmp.nsi >tmp2.nsi
	mv tmp2.nsi tmp.nsi


	echo $FOUND_DLLS | sed 's/ /\n/g' |  sed 's@^@  Delete \"$INSTDIR\\@' | sed 's/$/\"/' > tmp-insert
	perl -ne 's/^  ;INSERT_UNINST_DLLS_HERE/`cat tmp-insert$1`/e;print' tmp.nsi > tmp2.nsi
	mv tmp2.nsi tmp.nsi

	#TODO: Why is perl converting this to dos?
	dos2unix tmp.nsi

	NSI_FILE=tmp.nsi

	makensis `basename $NSI_FILE` ||  { echo "makensis failed" ; exit 1; }

	echo "-------------------"
	VERSION=`cat $NSI_FILE | grep "define PRODUCT_VERSION " | awk '{print $3}' | sed s/\"//g | sed s/\s*$//`
	if [ $IS_RELEASE -ne 0 ] ; then
		echo "Release mode enabled:"
		TARGET_FILE=3Depict-$VERSION-$HOST_EXT.exe
	else
		echo "Release mode disabled:"
		TARGET_FILE=3Depict-${VERSION}-${HOST_EXT}-debug.exe
	fi
	
	mv Setup.exe  $TARGET_FILE
	echo "File written to : `pwd`/$TARGET_FILE"
	echo "-------------------"
	
	popd > /dev/null
}

#Check that we have a suitable host system
checkHost

#Check we have the patches we need
checkPatchesExist

#build the dirs we need
createDirLayout


#Install cross compiler
#---
case ${HOST_VAL}  in
	x86_64-w64-mingw32)
		if [ x"$DIST_NAME" == x"Ubuntu" ] || [ x"$DIST_NAME" == x"LinuxMint" ] ; then
			MINGW_PACKAGES="mingw-w64-dev g++-mingw-w64-x86-64"
		else
			MINGW_PACKAGES="mingw-w64-x86-64-dev g++-mingw-w64-x86-64"
		fi
		HOST_EXT="win64"
	;;
	i686-w64-mingw32)
		MINGW_PACKAGES="gcc-mingw-w64-i686 g++-mingw-w64-i686"
		HOST_EXT="win32"
	;;
	*)
		echo "Unknown host... please install deps manually,or alter script"
		exit 1	
	;;
esac

#install the compiler
install_mingw
#---

#Create the binaries we need
createBaseBinaries

#Obtain the needed dependencies
grabDeps



#set our needed environment variables
PATH=${BASE}/bin/:/usr/$HOST_VAL/bin/:$PATH
export CXX=${HOST_VAL}-g++
export CPP=${HOST_VAL}-cpp
export CC=${HOST_VAL}-gcc
export CPPFLAGS=-I${BASE}/include/
export CFLAGS=-I${BASE}/include/
export CXXFLAGS=-I${BASE}/include/
export LDFLAGS=-L${BASE}/lib/
export RANLIB=${HOST_VAL}-ranlib
export LIBS="-L${BASE}/lib/"
DESTDIR=${BASE}

build_zlib
build_libtiff
build_libpng
build_libjpeg
build_libxml2
build_gsl
build_qhull
build_expat
build_freetype
build_libiconv
build_gettext 
build_ftgl 
build_glew
build_libvigra

build_mathgl 
build_wx

build_3Depict

make_package

