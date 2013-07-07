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

IS_RELEASE=1

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

PATCHES_GLEW="glew-makefile"

PATCH_LIST="$PATCHES_WXWIDGETS_PRE $PATCHES_WXWIDGETS_POST $PATCHES_GSL $PATCHES_ZLIB $PATCHES_LIBPNG $PATCHES_GETTEXT $PATCHES_FTGL $PATCHES_GLEW"

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
		if [ x`cat $PATCH_STATUS_FILE | grep "$i"` != x"" ] ; then
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

	#install mingw and libtool (we will need it...)

	GET_PACKAGES="";
	for i in $MINGW_PACKAGES
	do
		if [ x`apt-cache pkgnames --installed $i` != x"$i" ] ; then
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
	DEB_PACKAGES="expat freetype ftgl gettext gsl libjpeg8 libpng libxml2 mathgl qhull tiff3 wxwidgets2.8 zlib glew"

	GET_PACKAGES=""
	for i in $DEB_PACKAGES
	do
		FNAME=`ls packages/$i-*.orig.* 2> /dev/null`
		if [ x"$FNAME" == x"" ] ; then
			GET_PACKAGES="${GET_PACKAGES} $i"
		fi
	done

	#grab packages if they are not already on-disk
	if [ x"$GET_PACKAGES" != x"" ] ; then
		echo apt-get source $GET_PACKAGES
	fi

	if [ $? -ne 0 ] ; then
		echo "apt-get source failed... Maybe check internet connection, then try updating package database, then re-run?"
		exit 1
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
		mv $i*.*
	done


	#extract libiconv if needed
	#--
	LIBICONV=libiconv-1.14
	if [ ! -f packages/${LIBICONV}.tar.gz ] ; then
		wget "ftp://ftp.gnu.org/gnu/libiconv/libiconv-1.14.tar.gz" || { echo "Libiconv download failed "; exit 1; }
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
	sed -i "s@HOST_VAL@$HOST_VAL@" patches/glew-makefile
	sed -i "s@BASEDIR@$BASE@" patches/glew-makefile
	
	pushd deps >/dev/null
	pushd glew-* >/dev/null


	if [ $? -ne 0 ] ; then
		echo "glew dir missing, or duplicated?"
		exit 1
	fi
	
	make clean
	rm -f configure.log

	APPLY_PATCH_ARG="$PATCHES_GLEW"
	applyPatches

	LD=$CC make -j $NUM_PROCS || { echo "glew build failed"; exit 1; } 

	make install DESTDIR="$BASE"|| { echo "glew install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null

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
	pushd libxml2-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "libxml2 dir missing, or duplicated?"
		exit 1
	fi
	make clean

	#Modifications
	#	Disable python, because sys/select.h is not in mingw
	./configure --host=$HOST_VAL --without-python --enable-shared --disable-static --prefix=/ || { echo "Libxml2 configure failed"; exit 1; } 

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
	NAME="libjpeg"
	ISBUILT_ARG=${NAME}
	isBuilt
	if [ $ISBUILT -eq 1 ] ; then
		return;
	fi
	pushd deps >/dev/null
	pushd libjpeg[0-9]*-* >/dev/null
	
	if [ $? -ne 0 ] ; then
		echo "libjpeg dir missing, or duplicated?"
		exit 1
	fi

	make clean

	./configure --host=$HOST_VAL --enable-shared --disable-static --prefix=/ || { echo "Libjpeg configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "libjpeg build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "libjpeg install failed"; exit 1; } 
	
	#DLL needs to be copied into lib manually
	cp -p .libs/${NAME}-[0-9]*.dll $BASE/lib/ 

	popd >/dev/null
	popd >/dev/null

	FIX_LA_FILE_ARG=libjpeg
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
	pushd tiff[0-9]*-* >/dev/null
	
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

	./configure --host=$HOST_VAL --enable-shared --disable-static --prefix=/ || { echo "qhull configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "qhull build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "qhull install failed"; exit 1; } 

	popd >/dev/null
	popd >/dev/null
	
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
	./configure --host=$HOST_VAL --enable-shared --disable-static --prefix=/ || { echo "Libtiff configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "expat build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "expat install failed"; exit 1; } 

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
	ln -s `find ${BASE}/lib/wx/config/ -name \*release-2.8` wx-config
	APPLY_PATCH_ARG=$PATCHES_WXWIDGETS_POST
	PATCH_LEVEL=0
	applyPatches
	PATCH_LEVEL=1
	sed -i "s@REPLACE_BASENAME@${BASE}@" wx-config || { echo "Failed to update wx-config with build root,. Aborting";  exit 1; }
	popd

	pushd ./lib/
	ln -s wx-2.8/wx/ wx
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
	./configure --host=$HOST_VAL --enable-shared --disable-static --prefix=/ || { echo "freetype configure failed"; exit 1; } 

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
		echo "gettext dir missing, or duplicated?"
		exit 1
	fi

	make clean

	APPLY_PATCH_ARG=$PATCHES_GETTEXT
	applyPatches
	automake

	./configure --host=$HOST_VAL --enable-shared --disable-static --prefix=/ || { echo "gettext configure failed"; exit 1; } 

	make -j $NUM_PROCS || { echo "gettext build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "gettext install failed"; exit 1; } 

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

	libtoolize --copy --force
	aclocal

	autoreconf
	LIBS="${LIBS} -lz" ./configure --host=$HOST_VAL --disable-pthread --enable-shared --disable-static --prefix=/ || { echo "mathgl configure failed"; exit 1; } 

	#RPATH disable hack
	sed -i 's|^hardcode_libdir_flag_spec=.*|hardcode_libdir_flag_spec=""|g' libtool
	sed -i 's|^runpath_var=LD_RUN_PATH|runpath_var=DIE_RPATH_DIE|g' libtool

	#Hack to fix mathgl's libpng dll search
	mkdir -p $BASE/lib/.libs/
	ln -s $BASE/lib/libpng.dll $BASE/lib/.libs/libpng.dll.a 

	make -j $NUM_PROCS || { echo "mathgl build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "mathgl install failed"; exit 1; } 
	
	#DLL needs to be copied into lib manually
	cp -p .libs/${NAME}-[0-9]*.dll $BASE/lib/ 

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


	#rename GLU to glu32 and gl to opengl32
	sed -i s/'\-lGLU'/-lglu32/ configure
	sed -i s/'\-lGL'/-lopengl32/ configure

	APPLY_PATCH_ARG="$PATCHES_FTGL"
	applyPatches

	./configure --host=$HOST_VAL --enable-shared --disable-static --prefix=/ || { echo "ftgl configure failed"; exit 1; } 

	
	make -j $NUM_PROCS || { echo "ftgl build failed"; exit 1; } 
	
	make install DESTDIR="$BASE"|| { echo "ftgl install failed"; exit 1; } 

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
		
		DIST_NAME=`lsb_release -i | awk -F: '{print $2}' | sed 's/\s//g'`
		if [ x$DIST_NAME != x"Debian" ] ; then
			echo "This is meant to target debian. I'm not sure if it will work on your system. You'll need to work that out."
		fi
	fi
	
	echo "done"
}

#Build 3Depict
function build_3Depict()
{
	pushd code/3Depict
	make distclean


	CONF_FLAG="--host=$HOST_VAL"
	if [ $IS_RELEASE -ne 0 ] ; then
		CONF_FLAG="$CONF_FLAG --disable-debug-checks"
	fi

	CFLAGS="$CFLAGS -DUNICODE" CPPFLAGS="${CPPFLAGS} -DUNICODE" ./configure  $CONF_FLAG

	if [ $? -ne 0 ] ; then
		echo "Failed 3Depict configure"
		exit 1
	fi

	#HACK - strip all makefiles of -D_GLIBCXX_DEBUG
	find ./ -name Makefile -exec sed -i 's/-D_GLIBCXX_DEBUG//g' {} \;

	make -j$NUM_PROCS
	if [ $? -ne 0 ] ; then
		echo "Failed 3Depict build"
		exit 1
	fi

	if [ $IS_RELEASE -ne 0 ] ; then
		#Sanity check that we are actually in debug mode
		TEST_FLAG=`./3Depict --help  2>&1 | grep "\-\-test"`
		if [ x"$TEST_FLAG" != x"" ] ; then
			echo "3Depict\'s Unit tests available, but should not be, when in release mode" 
			exit 1
		fi
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

	NSI_FILE=./packaging/mingw-debian-cross/windows-installer.nsi
	if [ ! -f $NSI_FILE ] ; then
		echo "NSI file missing whilst trying to build package"
		exit 1;
	fi

	#copy as needed
	# Due to debian bug : #704828, makensis cannot correctly handle symlinks,
	# so don't use symlinks
	if [ ! -f `basename $NSI_FILE`  ] ; then
		cp ./packaging/mingw-debian-cross/windows-installer.nsi .
	fi


	echo -n " Copying dll files... "
	DLL_FILES=`grep File windows-installer.nsi | egrep '(\.dll|\.so)' | sed 's/.*src.//' | sed 's/\"//' | sed 's/\r/ /g'`

	SYS_DIR=/usr/lib/gcc/${HOST_VAL}/
	#copy the DLL files from system or 
	# from the build locations
	for i in ${DLL_FILES}
	do
		HAVE_DLL=0
		for j in ${BASE}/lib/ ${BASE}/bin/ $SYS_DIR
		do
			FIND_RES=`find $j -name $i | head -n 1`
			if [ x$FIND_RES != x"" ] ; then
				HAVE_DLL=1;
				cp $FIND_RES ./src/
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
	echo "done."

	
	if [ $IS_RELEASE -ne 0 ] ; then
		#Strip debugging information
		pushd src/ > /dev/null
		strip *.dll *.exe
		popd > /dev/null
	fi


	makensis `basename $NSI_FILE` ||  { echo "makensis failed" ; exit 1; }

	if [ $IS_RELEASE -ne 0 ] ; then
		VERSION=`cat $NSI_FILE | grep "define PRODUCT_VERSION " | awk '{print $3}' | sed s/\"//g | sed s/.$//`
		TARGET_FILE=3Depict-$VERSION-$HOST_EXT.exe
		mv Setup.exe  $TARGET_FILE
		echo "-------------------"
		echo "File written to : `pwd`/$TARGET_FILE"
		echo "-------------------"
	fi
	
	popd > /dev/null
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

#Install cross compiler
case ${HOST_VAL}  in
	x86_64-w64-mingw32)
		MINGW_PACKAGES="mingw-w64-x86-64-dev g++-mingw-w64-x86-64"
		HOST_EXT="win64"
	;;
	i686-w64-mingw32)
		MINGW_PACKAGES="gcc-mingw32"
		HOST_EXT="win32"
	;;
	*)
		echo "Unknown host... please install deps manually,or alter script"
		exit 1	
	;;
esac

install_mingw

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
build_wx	# I'm not sure I've done this 100% right. Check wx-config output 
build_freetype
build_libiconv
build_gettext 
build_mathgl 
build_ftgl 
build_glew

build_3Depict

make_package

