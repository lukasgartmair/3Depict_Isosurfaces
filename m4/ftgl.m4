
dnl @synopsis AX_CHECK_FTGL
dnl
dnl Check for the existence of FTGL, if it is found create FTGL_CFLAGS and FTGL_LIBS to locate the directory in
dnl which the FTGL files are located. When #including the headers, do *not* use <FTGL/ftgl.h>, but rather <ftgl.h>
dnl The FTGL flags will be updated to ensure that this ambiguitity is overcome this automatically. 
dnl 
dnl Finally FTGL checking does not include version compatability - as without pkg-config it is unclear how to check the version. 
dnl
dnl Tested platforms : Mac OS X 10.4.6 with ftgl installed via fink, Fedora core 7 i386. Only gcc has been checked. Requires g++ of some sort
dnl
dnl This script requires the pkg.m4 file

#This is broken without pkg-config!!
# Allow user to pass a manual dir for FTGL
#
AC_ARG_WITH([ftgl-prefix], [--with-ftgl-prefix : specify prefix dir for FTGL],
		ftgl_prefix="$withval", ftgl_config_prefix="")
#
# allow for disabling the pkg-config check
AC_ARG_WITH([ftgl-no-pkg], 
		 [--with-ftgl-no-pkg : don't use pkg-config to check for ftgl])

AC_DEFUN([AX_CHECK_FTGL], [ 
dnl AC_REQUIRE([PKG_CHECK_MODULES]) - I had to take this out, with it in it messes up confgure
dnl as the PKG_CHECK_MODULES is expanded with no arguments. I dont want to expand it, I just
dnl want to ensure that it is defined. never mind. 


AC_MSG_CHECKING([for ftgl])


FTGL_CFLAGS=""
FTGL_LIBS=""
if test "x$ftgl_prefix" != "x" ; then
	#use the supplied CFLAGS. assume LIBS
	FTGL_CFLAGS="-I$ftgl_prefix/include/ -L$ftgl_prefix/lib/"
	FTGL_LIBS="-lFTGL"
else
	
	HAVE_PKG=$(basename $(which pkg-config))


	if test $HAVE_PKG != x"pkg-config"  ; then
		manual_ftgl="yes" ;
	fi
		

	if test "x$with_ftgl_no_pkg" = "xyes" ; then
		AC_DEFINE([FTGL_NO_PKG_CONFIG], [1], [Dont use pkg-config to locate ftgl])
		#well the user doesn't want us to use pkg-config so dont.
		manual_ftgl=yes
	else 
		if  ! test x"$manual_ftgl" == x"yes"  ; then
			#
			#Use PKG_CONFIG to do the heavy lifting, must be greater than 2.0.0
			#
			PKG_CHECK_MODULES([FTGL], ftgl >= 2.0.0, [libftgl="yes"], [libftgl="no"])

			#Check to see if pkg-config did the job
			if test "x$libftgl" = "xno" ; then
				#dang, looks like we have to try a manual approach
				manual_ftgl=yes ;
			fi
		fi
	fi
	
fi


if test "x$manual_ftgl" = "xyes" ; then
	LIBS_ORIG="$LIBS"
	LIBS="$LIBS $FTGL_LIBS $LDFLAGS"

	if test x$FTGL_LIBS == x""  ; then
		FTGL_LIBS="-lftgl"
	fi

	#TODO: see if we can put in some more manual tests for a few common locations for ftgl?
	AC_CHECK_LIB(ftgl, ftglCreateSimpleLayout, 
		[], AC_MSG_ERROR([Couldnt find ftgl -- provide base dir or install pkg_config]),-lm)
	
	CFLAGS="$CFLAGS_ORIG"
	LIBS="$LIBS_ORIG"
fi

#
# If we get this far, we assume that FTGL_CFLAGS and FTGL_LIBS is set
# all that remains is to check to see if FTGL.h is in the CFLAGS dir or in
# the FTGL subdir 
#

dnl remember FTGL is a c++ library 
AC_LANG_PUSH(C++)

save_CPPFLAGS=$CPPFLAGS
CPPFLAGS=$FTGL_CFLAGS

AC_TRY_CPP([#include <FTGL.h>], 
      		[ ftgl_nodir=yes ], [ ftgl_nodir=no ])

if test "x$ftgl_nodir" != "xyes" ; then

AC_TRY_CPP([#include <FTGL/FTGL.h>], 
		[ ftgl_dir=yes ], [ ftgl_dir=no ])

	if test "x$ftgl_dir" = "xyes" ; then
		dnl This test is broken. If FTGL is in a default GCC search path, then this will
		dnl fail. We should test this again with the new CFLAGS and see if it works.
		dnl finally we need to then test 
		save_FTGL_CFLAGS=$FTGL_CFLAGS
		
		dnl Watch the gotcha, m4 appears ot process away the [] when parsing the
		dnl sed regexp
		#scrub the FTGL_CFLAGS for -I and then tack on /FTGL as needed
		FTGL_CFLAGS=`echo $FTGL_CFLAGS | sed -e 's/-I\([[a-z\\\/]]*\)/-I\1\/FTGL /'`
		
		CPPFLAGS=$FTGL_CFLAGS
		AC_TRY_CPP([#include <FTGL.h>], 
				[ ftgl_def_dir=yes ], [ ftgl_def_dir=no ])

		if test "x$ftgl_def_dir" = "xno" ; then
			#try retrieving and modifying gcc's search dirs. - OMG Nasty hack! gcc
			#has no nice output for include dirs - sigh. got this from gcc-help,
			#then parsed with greps and tr
			GCC_SEARCHDIRS=`cpp -x c++ -Wp,-v /dev/null 2>&1 | grep -v ^\# | grep -v "End of search list." | grep -v "ignoring" | tr \\\n \\ `
			ftgl_search_ok="no"
			for flag in $GCC_SEARCHDIRS
			do
				CPPFLAGS="-I$flag/FTGL/"
				AC_TRY_CPP([#include <FTGL.h>], 
					[ ftgl_def_dir=yes ], [ ftgl_def_dir=no ])
				echo "Checking for FTGL.h in: $CPPFLAGS, result : $ftgl_def_dir"
				if test "x$ftgl_def_dir" = "xyes"; then
					ftgl_search_ok="yes"
					FTGL_CFLAGS="-I$flag/FTGL/"
					break
				fi
			done

			if test "x$ftgl_search_ok" = "xno"; then
				AC_MSG_ERROR([Unfortuntately whilst this script worked out that you have ftgl,  it couldn't quite work out how to set the CFLAGS]) 
			fi
		fi
	fi
fi

dnl return the cflags
CPPFLAGS=$save_CPPFLAGS
AC_LANG_POP([C++])

dnl #
dnl # Also There can exist variations on the naming of the library, some use libFTGL other libftgl 
dnl # So check that and see if that needs fixing
dnl #
dnl 
dnl AC_TRY_LINK([#include <FTGL.h>], 
dnl       		[ ftgl_lcase=yes ], [ ftgl_lcase=no ])
dnl 
dnl let configure know we are good to go
AC_MSG_RESULT([ yes ])

AC_SUBST(FTGL_CFLAGS)
AC_SUBST(FTGL_LIBS)

])
