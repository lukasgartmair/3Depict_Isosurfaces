#!/bin/bash
BASE=`pwd`

BUILD_STATUS_FILE="$BASE/build-status"
PATCH_STATUS_FILE="$BASE/patch-status"

PATCHES_WXWIDGETS_PRE="" #wxWidgets-2.8.12-mingw64-1.patch configure-wxbool-patch"
PATCHES_WXWIDGETS_POST="wx-config-sysroot.patch"

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

PREFIX=/
NUM_PROCS=4

if [ `id -u` -eq 0 ]; then
	echo "This script should not be run as root."
	echo " If you know what you are doing, you can disable this check, but be aware you _can_ break your system by doing this."
	exit 1;
fi

function applyPatches()
{
	for i in $APPLY_PATCH_ARG
	do
		if [ x"`cat $PATCH_STATUS_FILE | grep "$i"`" != x"" ] ; then
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
function isBuilt()
{
	if [ x`cat ${BUILD_STATUS_FILE} | grep $ISBUILT_ARG` != x"" ]  ; then
		ISBUILT=1
	else
		ISBUILT=0
	fi
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
	pushd wxWidgets-3.0* >/dev/null
       	
	if [ $? -ne 0 ] ; then
		echo "wxwidgets dir missing, or duplicated?"
		exit 1
	fi

	make clean

#	APPLY_PATCH_ARG=$PATCHES_WXWIDGETS_PRE
#	applyPatches
#WX_DISABLE="--disable-compat26 --disable-ole --disable-dataobj --disable-ipc --disable-apple_ieee --disable-zipstream --disable-protocol_ftp --disable-mshtmlhelp --disable-aui --disable-mdi --disable-postscript --disable-datepick --disable-splash --disable-wizarddlg --disable-joystick --disable-loggui --disable-debug --disable-logwin --disable-logdlg --disable-tarstream --disable-fs_archive --disable-fs_inet --disable-fs_zip --disable-snglinst --disable-sound --without-regex --disable-svg --disable-webview --disable-markup --disable-banner-window --disable-calendar --disable-mediactrl --disable-stc --disable-timepick --disable-treebook --disable-toolbook --disable-finddlg --disable-prefseditor --disable-editablebox  --disable-compat28 --disable-ftp --disable-ole --disable-arcstream --disable-dialupman --disable-sound --disable-protocol_ftp --disable-html --disable-htmlhelp --disable-ribbon --disable-tarstrema --disable-zipstream --disable-propgrid --disable-richtext --disable-richtooltip"

	./configure --host=$HOST_VAL --enable-monolithic --disable-compat28 --disable-propgrid --enable-shared --disable-static --with-opengl --enable-msw --prefix=/ || { echo "wxwidgets configure failed"; exit 1; } 

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
	cp `find ${BASE}/lib/wx/config/ -name \*unicode-3.0` wx-config
	APPLY_PATCH_ARG=$PATCHES_WXWIDGETS_POST
	PATCH_LEVEL=0
#applyPatches
	PATCH_LEVEL=1
	sed -i "s@REPLACE_BASENAME@${BASE}@" wx-config || { echo "Failed to update wx-config with build root,. Aborting";  exit 1; }
	popd

	pushd ./lib/
	ln -s wx-2.8/wx/ wx
	popd


	echo ${NAME} >> $BUILD_STATUS_FILE

}

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
		MINGW_PACKAGES="gcc-mingw32"
		HOST_EXT="win32"
	;;
	*)
		echo "Unknown host... please install deps manually,or alter script"
		exit 1	
	;;
esac


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

build_wx	# I'm not sure I've done this 100% right. Check wx-config output 
