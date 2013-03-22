#!/bin/bash

#Script to bootstrap 3Depict cross-compilation under debian
# and debian-like systems	

BASE=`pwd`
HOST_VAL=x86_64-w64-mingw32
PREFIX=/
NUM_PROCS=4

if [ `id -u` -eq 0 ]; then
	echo "This script should not be run as root."
	echo " If you know what you are doing, you can disable this check, but be aware you _can_ break your system by doing this."
	exit 1;
fi
#1) Filezilla wxwidgets patch for 64 bit support under mingw
#2) own patch for fixing wx-config's lack of sysroot support
PATCHES_WXWIDGETS_PRE="wxWidgets-2.8.12-mingw64-1.patch"
PATCHES_WXWIDGETS_POST="wx-config-sysroot.patch"
#1) Zlib no longer needs to explicitly link libc, and will fail if it tries
PATCHES_ZLIB="zlib-no-lc.patch"
#1) Override some configure patches to bypass false positive failures
PATCHES_FTGL="ftgl-override-configure"

#1) gettext-tools fails in various places, but we don't actually need it, so turn it off
#2) gettext fails to correctly determine windows function call prefix.
#   should be fixed for gettext > 0.18.1.1 ?
#   https://lists.gnu.org/archive/html/bug-gettext/2012-12/msg00071.html
PATCHES_GETTEXT="gettext-disable-tools gettext-win32-prefix"

PATCH_LIST="$PATCHES_WXWIDGETS $PATCHES_GSL $PATCHES_ZLIB $PATCHES_LIBPNG"


function applyPatches()
{
	for i in $APPLY_PATCH_ARG
	do
		echo "Applying patch:" $i
		patch -t -p1  < ../../patches/$i
		
		if [ $? -ne 0 ] ; then
			echo "Failed applying patch :" $i
			exit 1
		fi
		echo "applied patch"
		sleep 1
	done
}

function installX86_64_mingw32()
{
	MINGW_PACKAGES="mingw-w64-x86-64-dev g++-mingw-w64-x86-64"
	echo "Requesting install of w64-mingw :" $MINGW_PACKAGES

	#install mingw and libtool (we will need it...)
	sudo apt-get install $MINGW_PACKAGES libtool

	if [ $? -ne 0] ; then
		echo "Mingw installation failed".
		exit 1
	fi
}

function grabDeps()
{
	pushd deps 2>/dev/null
	DEB_PACKAGES="expat freetype ftgl gettext gsl libjpeg8 libpng libxml2 mathgl qhull tiff3 wxwidgets2.8 zlib"
	apt-get source $DEB_PACKAGES

	if [ $? -ne 0 ] ; then
		echo "apt-get source failed... Maybe check internet connection, then try updating package database, then re-run?"
		exit 1
	fi

	mv *.orig.* *.debian.* *.dsc *.diff.* packages 

	if [ ! -f libiconv-1.14.tar.gz ] ; then
		wget "ftp://ftp.gnu.org/gnu/libiconv/libiconv-1.14.tar.gz" || { echo "Libiconv download failed "; exit 1; }
	else
		if [ `sha1sum libiconv-1.14.tar.gz` != be7d67e50d72ff067b2c0291311bc283add36965 ] ; then
			echo "SHA1 sum mismatch for libiconv"
			exit 1
		fi
	fi
	
	tar -zxf libiconv-1.14.tar.gz || { echo "failed extracting iconv "; exit 1; }

	
	popd 2> /dev/null

}

function createBaseBinaries()
{
	pushd bin >/dev/null

	#Create symlinks to the compiler, which will be in our
	# primary path, and thus override the normal compiler
	
	#use ccache if it is around
	if [ x`which ccache` != x"" ] ; then
		echo -n "Enabling ccache support..."
		USE_CCACHE=1
	else
		echo -n "Making direct compiler symlinks..."
		USE_CCACHE=0
	fi

	for i in g++ cpp gcc c++
	do
		#Skip existing links
		if [ -f ${HOST_VAL}-$i ] ; then
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
	pushd deps >/dev/null
	pushd zlib-* >/dev/null


	if [ $? -ne 0 ] ; then
		echo "Zlib dir missing, or duplicated?"
		exit 1
	fi
	
	make clean
	rm -f configure.log

	
	./configure  --prefix="/" || { echo "ZLib configurefailed"; exit 1; } 
	
	APPLY_PATCH_ARG="$PATCHES_ZLIB"
	applyPatches

	make -j $NUM_PROCS || { echo "ZLib build failed"; exit 1; } 

	make install DESTDIR="$BASE"|| { echo "ZLib install failed"; exit 1; } 

	#Remove the static zlib. We don't want it
	rm $BASE/lib/libz.a
	popd >/dev/null
	popd >/dev/null
}

function build_libpng()
{
	pushd deps >/dev/null
	pushd libpng-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "libpng dir missing, or duplicated?"
		exit 1
	fi

	make clean

	./configure --host=x86_64-w64-mingw32 --enable-shared --disable-static || { echo "Libpng configure failed"; exit 1; } 

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


	popd >/dev/null
	popd >/dev/null

	FIX_LA_FILE_ARG=libpng
	fix_la_file
}

function build_libxml2()
{
	pushd deps >/dev/null
	pushd libxml2-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "libxml2 dir missing, or duplicated?"
		exit 1
	fi
	make clean

	./configure --host=x86_64-w64-mingw32 --enable-shared --disable-static --prefix=/ || { echo "Libxml2 configure failed"; exit 1; } 

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
}

function build_libjpeg()
{
	pushd deps >/dev/null
	pushd libjpeg[0-9]*-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "libjpeg dir missing, or duplicated?"
		exit 1
	fi

	make clean

	./configure --host=x86_64-w64-mingw32 --enable-shared --disable-static --prefix=/ || { echo "Libjpeg configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "libjpeg build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "libjpeg install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null

	FIX_LA_FILE_ARG=libjpeg
	fix_la_file
}

function build_libtiff()
{
	pushd deps >/dev/null
	pushd tiff[0-9]*-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "libtiff dir missing, or duplicated?"
		exit 1
	fi
	./configure --host=x86_64-w64-mingw32 --enable-shared --disable-static --prefix=/ || { echo "Libtiff configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "libtiff build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "libtiff install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null
	
	FIX_LA_FILE_ARG=libtiff
	fix_la_file
}

function build_qhull()
{
	pushd deps >/dev/null
	pushd qhull-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "qhull dir missing, or duplicated?"
		exit 1
	fi

	make clean

	./configure --host=x86_64-w64-mingw32 --enable-shared --disable-static --prefix=/ || { echo "qhull configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "qhull build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "qhull install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null
	
	FIX_LA_FILE_ARG=libqhull
	fix_la_file
}


function build_expat()
{
	pushd deps >/dev/null
	pushd expat-** >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "expat dir missing, or duplicated?"
		exit 1
	fi
	./configure --host=x86_64-w64-mingw32 --enable-shared --disable-static --prefix=/ || { echo "Libtiff configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "expat build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "expat install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null
	FIX_LA_FILE_ARG=libexpat
	fix_la_file
}

function build_gsl()
{
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

	./configure --host=x86_64-w64-mingw32 --enable-shared --disable-static --prefix=/ || { echo "gsl configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "gsl build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "gsl install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null


	FIX_LA_FILE_ARG=libgsl
	fix_la_file
}

function build_wx()
{
	pushd deps >/dev/null
	pushd wxwidgets[23].[0-9]-* >/dev/null
		
	if [ $? -ne 0 ] ; then
		echo "wxwidgets dir missing, or duplicated?"
		exit 1
	fi

	make clean

	APPLY_PATCH_ARG=$PATCHES_WXWIDGETS_PRE
	applyPatches
	./configure --host=x86_64-w64-mingw32 --enable-shared --disable-static --with-opengl --enable-unicode --without-regex --prefix=/ || { echo "wxwidgets configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "wxwidgets build failed"; exit 1; } 
	
	APPLY_PATCH_ARG=$PATCHES_WXWIDGETS_POST
	applyPatches
	make install DESTDIR="$BASE"|| { echo "wxwidgets install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null
}

function build_freetype()
{
	pushd deps >/dev/null
	pushd freetype-* >/dev/null
		
	if [ $? -ne 0 ] ; then
		echo "freetype dir missing, or duplicated?"
		exit 1
	fi

	tar -xjf freetype-[0-9]*bz2  || { echo "freetype decompress failed" ; exit 1; } 

	pushd freetype-[0-9]*
	make clean
	./configure --host=x86_64-w64-mingw32 --enable-shared --disable-static --prefix=/ || { echo "freetype configure failed"; exit 1; } 

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
}


function build_libiconv()
{
	pushd deps >/dev/null
	pushd libiconv-* >/dev/null
		
	if [ $? -ne 0 ] ; then
		echo "libiconv dir missing, or duplicated?"
		exit 1
	fi

	make clean
	./configure --host=x86_64-w64-mingw32 --enable-shared --disable-static --prefix=/ || { echo "libiconv configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "libiconv build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "libiconv install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null
	
	FIX_LA_FILE_ARG=libiconv
	fix_la_file
}

function build_gettext()
{
	pushd deps >/dev/null
	pushd gettext-* >/dev/null
		
	if [ $? -ne 0 ] ; then
		echo "gettext dir missing, or duplicated?"
		exit 1
	fi

	make clean

	APPLY_PATCH_ARG=$PATCHES_GETTEXT
	applyPatches
	automake

	./configure --host=x86_64-w64-mingw32 --enable-shared --disable-static --prefix=/ || { echo "gettext configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "gettext build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "gettext install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null
	
	FIX_LA_FILE_ARG=libintl
	fix_la_file
	FIX_LA_FILE_ARG=libcharset
	fix_la_file
}

function build_mathgl()
{
	pushd deps >/dev/null
	pushd mathgl-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "mathgl dir missing, or duplicated?"
		exit 1
	fi
	make clean

	libtoolize --copy --force
	aclocal

	autoreconf
	LIBS="${LIBS} -lz" ./configure --host=x86_64-w64-mingw32 --disable-pthread --enable-shared --disable-static --prefix=/ || { echo "mathgl configure failed"; exit 1; } 

	#RPATH disable hack
	sed -i 's|^hardcode_libdir_flag_spec=.*|hardcode_libdir_flag_spec=""|g' libtool
	sed -i 's|^runpath_var=LD_RUN_PATH|runpath_var=DIE_RPATH_DIE|g' libtool
	
	make -j $NUM_PROCS || { echo "mathgl build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "mathgl install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null
	FIX_LA_FILE_ARG=libmgl
	fix_la_file
}

function build_ftgl()
{
	pushd deps >/dev/null
	pushd ftgl-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "ftgl dir missing, or duplicated?"
		exit 1
	fi
	make clean


	#rename GLU to glu32 and gl to opengl32
	sed -i s/'\-lGLU'/-lglu32/ configure
	sed -i s/'\-lGL'/-lopengl32/ configure

#	APPLY_PATCH_ARG="$PATCHES_FTGL"
#	applyPatches

	./configure --host=x86_64-w64-mingw32 --enable-shared --disable-static --prefix=/ || { echo "ftgl configure failed"; exit 1; } 

	
	make -j $NUM_PROCS || { echo "ftgl build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "ftgl install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null
	FIX_LA_FILE_ARG=libftgl
	fix_la_file
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
		echo "This only works on x86_64 - you'll need to modify the script to handle other arches. Sorry."
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
		
		DIST_NAME=`lsb_release -i | awk -F: '{print $2}' | sed 's/\s//g'`
		if [ x$DIST_NAME != x"Debian" ] ; then
			echo "This is meant to target debian. I'm not sure if it will work on your system. You'll need to work that out."
		fi
	fi
	
	echo "done"
}

#Check that we have a suitable host system
checkHost

#Check we have the patches we need
checkPatchesExist

#build the dirs we need
createDirLayout

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
#export LDFLAGS=-L${BASE}/lib/
export RANLIB=${HOST_VAL}-ranlib
export LIBS=" -L${BASE}/lib/"
DESTDIR=${BASE}

build_zlib
build_libtiff
build_libpng
build_libjpeg
build_libxml2
build_gsl
build_qhull
build_expat
build_wx	# I'm not sure I've done this 100% right. Check wx-config output 
build_freetype
build_libiconv
build_gettext 
build_mathgl 
build_ftgl 

#cd code/3Depict
#CFLAGS="$CFLAGS -DUNICODE" CPPFLAGS="${CPPFLAGS} -DUNICODE" ./configure --host=$HOST_VAL
#make -j4


