AC_INIT([3Depict], [0.0.19]) 
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_PROG_CXX
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

m4_include([m4/ax_compare_version.m4])

#Ok, lets try using gcc style precompiled headers.
AM_CONDITIONAL(USE_PRECOMPILED_HEADERS, 1)

#Check sizeof
AC_CHECK_SIZEOF(size_t)

# Support {host_os} variable
AC_CANONICAL_HOST

dnl Test for wx-widgets
dnl ----------
AM_OPTIONS_WXCONFIG

case "${host_os}" in
	*mingw*)
		#Windows requires 3.1, as there is a bug with wxPropertyGrid modification during events
		# under GTK we use a hack to workaround, by keeping two grids, and flipping them in and out. 
		#This doesn't work properly under windows. wx3.1 is not available under linux  
		# (using repositories) at this time.
		AM_PATH_WXCONFIG(3.1.0, wxWin=1)
		;;	
	*)
		AM_PATH_WXCONFIG(3.0.0, wxWin=1)
esac
		
if test "$wxWin" != 1; then
AC_MSG_ERROR([
		wxWidgets must be installed on your system
		but wx-config script couldnt be found.

		Please check that wx-config is in path, the directory
		where wxWidgets libraries are installed (returned by
		'wx-config --libs' command) is in LD_LIBRARY_PATH or
		equivalent variable and wxWidgets version is above the minimum specified above.
])
fi


#Append the --gl-libs flag
WX_LIBS="$WX_LIBS `$WX_CONFIG_PATH --libs gl,propgrid`"

WANT_WINDRES="no"
case "${host_os}" in 
	*mingw*)
		#wx-config is a little unreliable in cross-compile mode
		# Manually append -DUNICODE to cppflags/cxxflags
		WX_CXXFLAGS="$WX_CXXFLAGS -DUNICODE"
		WX_CPPFLAGS="$WX_CPPFLAGS -DUNICODE"
		WANT_WINDRES="yes"
		;;
esac

AC_MSG_RESULT([Want windres... $WANT_WINDRES])

AM_CONDITIONAL([HAVE_WINDRES], [ test x"$WANT_WINDRES" == "xyes"] )


AC_SUBST(WX_LIBS)
dnl ----------


# Check for xml-config  (libxml2 configuration utility)
# ------------
# Test from icecast library (GPL)
AC_ARG_WITH(xml-config,
  [  --with-xml-config=PATH  use xml-config in PATH to find libxml ])

have_xml="no"
if test "$with_xml_config" != "no"
then
  if test -n "$with_xml_config" -a "$with_xml_config" != "yes"
  then
    XMLCONFIG="$with_xml_config"
    if ! test -x "$XMLCONFIG"
    then
      AC_MSG_ERROR([$with_xml_config cannot be run])
    fi
  else
    AC_CHECK_PROGS(XMLCONFIG, [xml2-config xml-config])
  fi

  if test -n "$XMLCONFIG"
  then
    AC_DEFINE(HAVE_LIBXML, 1, [Define if you have the GNOME XML library])
    have_xml="yes"
  else
    AC_MSG_ERROR([Unable to locate the configuration utility xml-config: specify with --with-xml-config, or ensure it is your PATH environment variable])
  fi
  XML_LIBS="`$XMLCONFIG --libs`"
  XML_CFLAGS="`$XMLCONFIG --cflags`"

  AC_SUBST(XML_CFLAGS)
  AC_SUBST(XML_LIBS)

fi
AM_CONDITIONAL(USE_XML, test "$have_xml" = "yes")
#----------------

# Check for FT2
dnl ----------
#FT2 teset from the graphviz library configure.ac
FREETYPE_DIR="yes" 
AC_ARG_WITH(freetype,
	[  --with-freetype=DIR     where to find the freetype 2.x library],
       FREETYPE_DIR=$withval)

if test "x$FREETYPE_DIR" = "xno"; then
  AC_MSG_ERROR(FREETYPE2 library disabled)
else
                                                                              
  if test "x$FREETYPE_DIR" != "xyes"; then
    AC_PATH_PROG(FREETYPE_CONFIG,freetype-config,,[$FREETYPE_DIR/bin:$PATH])
  else
    AC_PATH_PROG(FREETYPE_CONFIG,freetype-config)
  fi

  if test -n "$FREETYPE_CONFIG"; then
    if test "x$FREETYPE_DIR" != "xyes"; then
      FT_INCLUDES="`$FREETYPE_CONFIG --cflags` -I$FREETYPE_DIR/include"
    else
      FT_INCLUDES=`$FREETYPE_CONFIG --cflags`
    fi
    ft_libtool=`$FREETYPE_CONFIG --libtool`
    # check that it really exists (FreeBSD apparently forgot to insttall it!)
    if test -f "$ft_libtool"; then
	FT_LIBTOOL="$ft_libtool"
    else
	FT_LIBTOOL=""
    fi
    FT_LIBS=`$FREETYPE_CONFIG --libs`
    FT_LDFLAGS=`echo " $FT_LIBS" |sed 's/ -l[[^ ]][[^ ]]*//g'`
  else
    if test "x$FREETYPE_DIR" != "xyes"; then
      FT_INCLUDES="-I$FREETYPE_DIR/include/freetype2 -I$FREETYPE_DIR/include"
      FT_LDFLAGS="-L$FREETYPE_DIR/lib"
      FT_LIBS="-lfreetype"
    else
      FT_INCLUDES=""
      FT_LDFLAGS=""
      FT_LIBS=""
    fi
    FT_LIBTOOL=""
  fi

  save_CPPFLAGS=$CPPFLAGS
  save_LDFLAGS=$LDFLAGS
  CPPFLAGS="$CPPFLAGS $FT_INCLUDES"
  LDFLAGS="$LDFLAGS $FT_LDFLAGS"
  AC_CHECK_HEADERS(ft2build.h)
  if test `eval echo '${'$as_ac_Header'}'` = yes; then
	AC_CHECK_LIB(freetype,main,
		[FT_LIBS="$FT_LIBS"
		AC_DEFINE_UNQUOTED(HAVE_LIBFREETYPE,1,[Define if you have the FREETYPE2 library])],
		[AC_MSG_ERROR(Error: FREETYPE2 library not available - no libfreetype.)])
  else
	AC_MSG_ERROR(Required FREETYPE2 library not available - no ft2build.h)
  fi
  CPPFLAGS=$save_CPPFLAGS
  LDFLAGS=$save_LDFLAGS
  AC_SUBST(FT_INCLUDES)
  AC_SUBST(FT_LIBS)
fi


dnl ----------

#Check for FTGL using custom script.
dnl ----------
AX_CHECK_FTGL() #Not doing anything???

AC_SUBST(FTGL_CFLAGS)
AC_SUBST(FTGL_LIBS)

dnl ----------

#Check for qhull
dnl--------------
AC_ARG_WITH(libqhull-flags,
  [  --with-libqhull-flags=PATH : specify compiler flags for libqhull])
AC_ARG_WITH(libqhull-link,
  [  --with-libqhull-link=PATH : specify linker flag (library) for libqhull])

if test x"$with_libqhull_flags" != x"" ; then
    QHULL_CFLAGS="$with_libqhull_flags"
fi
AC_SUBST([QHULL_CFLAGS])

#Attempt to compile a test program
CFLAGS_ORIG="$CFLAGS"
CFLAGS="$CFLAGS $QHULL_CFLAGS"
AC_CHECK_HEADER([qhull/qhull_a.h],[AC_DEFINE(HAVE_QHULL,[],[Have got libqhull headers])],
	[AC_MSG_ERROR([Required libqhull headers not found (looking for qhull/qhull_a.h)])])

if test x"$with_libqhull_link" != x"" ;
then
	QHULL_LIBS="$with_libqhull_link"
else
	QHULL_LIBS="-lqhull"
fi
LIBS_ORIG="$LIBS"
LIBS="$LIBS $QHULL_LIBS $LDFLAGS"
AC_CHECK_LIB(qhull, qh_qhull, [AC_DEFINE(HAVE_QHULL,[],[qhull compilation OK])] , AC_MSG_ERROR([You must have libqhull installed and be able to compile a sample program. compiler flags can be set using the with-libqhull-flags and with-libqhull-link parameters ]))
CFLAGS="$CFLAGS_ORIG"
LIBS="$LIBS_ORIG"
AC_SUBST([QHULL_LIBS])	
dnl--------------



#Check for libpng
dnl--------------
AC_ARG_WITH(libpng-flags,
  [  --with-libpng-flags=PATH : specify compiler flags for libpng])
AC_ARG_WITH(libpng-link,
  [  --with-libpng-link=PATH : specify linker flag (library) for libpng])

#set libpng's compiler flags
if test x"$with_libpng_flags" != x"" ;
then
    PNG_CFLAGS="$with_libpng_flags"
fi
AC_SUBST(PNG_CFLAGS)

#set libpng's link flags
if test x"$with_libpng_link" != x"" ;
then
    PNG_LIBS="$with_libpng_link"
else

	AC_CHECK_LIB([png],[png_create_write_struct_2] , 
		[PNG_LIBS=-lpng], [PNG_USE_PKG_CFG=yes],-lm)

	if test x"$PNG_USE_PKG_CFG" == x"yes" ; then
		PKG_CHECK_MODULES(PNG, libpng >= 1.2)
	fi
fi

AC_SUBST(PNG_LIBS)

#Attempt to compile a test program
CFLAGS_ORIG="$CFLAGS"
CFLAGS="$CFLAGS $PNG_CFLAGS"
AC_CHECK_HEADER([png.h],[AC_DEFINE(HAVE_PNG,[],[Have got libpng headers])],
	[AC_MSG_ERROR([Required libpng headers not found (looking for png.h])])

LIBS_ORIG="$LIBS"
LIBS="$LIBS $PNG_LIBS $LDFLAGS"
AC_CHECK_LIB(png, png_sig_cmp, [AC_DEFINE(HAVE_PNG,[],[PNG compilation OK])] , AC_MSG_ERROR([You must have libpng installed and be able to compile sample program]), -lz -lm)
CFLAGS="$CFLAGS_ORIG"
LIBS="$LIBS_ORIG"
	
dnl--------------


#Check for opengl
#------------


#Try linking against gluSphere windows usese -lglu32; mac?? ; linux -lGLU
case "${host_os}" in 
	mingw*|windows*|winnt)
		#win32 opengl names
		GL_LIBS="-lglu32 -lopengl32"
		#Add GLEW dependency for opengl > 1.1
		GL_LIBS="$GL_LIBS -lglew32"
		AC_SUBST(GL_LIBS)
		AC_PROG_LN_S 
		AC_PROG_RANLIB 
	    ;;
	darwin*)
		#This is handled by the --with-apple-opengl-framework 
		#option already. Nothing to do here

	    ;;
	 *)
		AC_CHECK_LIB(GLU, gluSphere, GL_LIBS="$GL_LIBS -lglu", [AC_MSG_ERROR([Could not find GLU library])])
		GL_LIBS="-lGL -lGLU"
		AC_SUBST(GL_LIBS)

	   ;;
esac


#------------

#Check for Mathgl
dnl ----------
CFLAGS_ORIG="$CFLAGS"
LDFLAGS_ORIG="$LDFLAGS"


AC_ARG_WITH(mgl-flags,
  [  --with-mgl-flags=PATH : specify compiler flags for mathgl])
if test x"$with_mgl_flags" != x"" ; then
	MGL_CFLAGS="$with_mgl_flags"
	AC_SUBST(MGL_CFLAGS)
fi
CFLAGS="$CFLAGS $MGL_CFLAGS"

AC_ARG_WITH(mgl-libs,
  [  --with-mgl-libs=PATH : specify linker flag (library) for mathgl])
if test x"$with_mgl_libs" != x"" ; then
	MGL_LIBS="$with_mgl_libs"
else
	MGL_LIBS="-lmgl"
fi
AC_SUBST(MGL_LIBS)
LDFLAGS="$LDFLAGS $MGL_LIBS"

#  mathgl2.x uses mgl_cf.h for c functions.
AC_LANG_PUSH([C++])
AC_DEFINE(USE_MGL2, 1 , ["Enable mgl2 support"])
AC_CHECK_HEADER("mgl2/mgl_cf.h",[],[AC_MSG_ERROR(["mgl2 specified, but header mgl2/mgl_cf.h not found"])],[])
AC_LANG_POP([C++])


CFLAGS="$CFLAGS_ORIG"
LDFLAGS="$LDFLAGS_ORIG"

dnl -------

#Check for gsl
dnl -------

AC_ARG_WITH(gsl-flags,
  [  --with-gsl-flags=PATH : specify compiler flags for gsl])
AC_ARG_WITH(gsl-libs,
  [  --with-gsl-libs=PATH : specify linker flag (library) for gsl])

if test x"$with_gsl_flags" != x"" ; then
    GSL_CFLAGS="$with_gsl_flags"
fi
if test x"$with_gsl_libs" != x"" ; then
    GSL_LIBS="$with_gsl_libs"
else
	AX_PATH_GSL([1.1],[],[AC_MSG_WARN(["Could not find GNU Scientific Library.. You should install this, as it is needed by mathgl. Otherwise, you can override it with --with-gsl-link and --with-gsl-flags. For example you might use the gsl-config program to spit out the needed libs."])])
fi

dnl -----------

#Check for libintl
dnl -----------------

AC_MSG_CHECKING( [ for libintl ] );

AC_ARG_WITH(intl-libs,
  [  --with-intl-libs=FLAGS: specify linker flags (library) for internationalisation libs])

if test x"$with_intl_libs" != x"" ; then
    GETTEXT_LIBS="$with_intl_libs"
    AC_SUBST(GETTEXT_LIBS)
    AC_MSG_RESULT( [ specified ]);
else
	case "${host_os}" in 
		darwin*)
			#Darwin requires explicit libintl
			GETTEXT_LIBS="-lintl -liconv"
			AC_SUBST(GETTEXT_LIBS)
    			AC_MSG_RESULT( [ $GETTEXT_LIBS ]);
		    ;;
		mingw*|windows*|winnt)
			GETTEXT_LIBS="-lintl"
			AC_SUBST(GETTEXT_LIBS)
    			AC_MSG_RESULT( [ $GETTEXT_LIBS ]);
		    ;;

		 *)
			#elsewhere it appears to be part of libc. or something.
			AC_MSG_RESULT( [ no ]);
		   ;;
	esac
fi


dnl -----------------


dnl -----------------
AC_LANG_PUSH([C++])
AC_CHECK_HEADER([vigra/multi_array.hxx],[AC_DEFINE(HAVE_VIGRA,[],[Have got vigra headers])],
	[AC_MSG_ERROR([Required vigra headers not found (looking for vigra/multi_array.hxx)])])

AC_LANG_POP([C++])
dnl -----------------


#Should we compile with openMP?
AC_ARG_ENABLE(openmp-parallel,
  [  --enable-openmp-parallel  Enable OpenMP multi-CPU usage; requires GCC > 4.2 for parallel STL support ])
#Should we enable or disable debug checking?
AC_ARG_ENABLE(debug-checks,
  [  --disable-debug-checks Disable any debug checking, provides faster operation, but less information needed to debug internal problems, or to provide problem reports to developers ],[enable_debug_checks="no"],[enable_debug_checks="yes"])

AC_ARG_ENABLE(ubsan,
	[ --disable-ubsan Disable undefined behaviour sanitizer. Only takes effect on certain ubsan supporting compilers. Useful for working around ubsan aborts that you cant fix (eg 3rd party libs ],
		[ enable_no_ubsan="yes"],[enable_no_ubsan="no"])

AC_ARG_ENABLE(experimental-cpp11,
  [  --enable-experimental-cpp11 Enable experimental C++11 support. Requires a full C++11 compiler (eg gcc/clang).])


if test x"$enable_openmp_parallel" != x"" ; 
then
	OPENMP_FLAGS="-fopenmp -D_GLIBCXX_PARALLEL"
	AC_SUBST(OPENMP_FLAGS)
fi

if test x"$enable_debug_checks" == x"yes" ; 
then

	DEBUG_FLAGS="-DDEBUG"

	AC_SUBST(DEBUG_FLAGS)

	if test x"$GCC" == x"yes" ; then
		# Strip optimisation flags from debug build
		#--
		changequote({,})
		CFLAGS=`echo "$CFLAGS" | sed -e 's/-O[0-9s]*//g'`
		CXXFLAGS=`echo "$CXXFLAGS" | sed -e 's/-O[0-9s]*//g'`
		changequote([,])
		CFLAGS="$CFLAGS -g"
		CXXFLAGS="$CXXFLAGS -g"
		#--

		#Check for UbSan availability
		#--
		if test x"${enable_no_ubsan}" == x"no" ; then

			USE_UBSAN="no"

			case "${host_os}" in
				*inux*)
					GCC_VER=`gcc --version | head -n 1 | awk '{ print $(NF)}'`
					AX_COMPARE_VERSION( [ $GCC_VER ], ge, 4.9,
							[ USE_UBSAN="yes" ] , [ USE_UBSAN="no" ])
					AX_COMPARE_VERSION( [ $GCC_VER ], ge, 6.0,
							[ USE_UBSAN_GCC6="yes" ] , [ USE_UBSAN_GCC6="no" ])
				;;
			esac

			if test x"$USE_UBSAN" == x"yes" ; then
				CFLAGS="$CFLAGS  -fsanitize=address -fsanitize=undefined -fsanitize=return -fsanitize=integer-divide-by-zero -fsanitize=vla-bound -fsanitize=null -fsanitize=signed-integer-overflow"
				CXXFLAGS="$CXXFLAGS  -fsanitize=address -fsanitize=undefined -fsanitize=return -fsanitize=integer-divide-by-zero -fsanitize=vla-bound -fsanitize=null -fsanitize=signed-integer-overflow"
				LDFLAGS="$LDFLAGS  -fsanitize=address -fsanitize=undefined -fsanitize=return "
				AC_MSG_RESULT(["Enabling Gcc-UbSan"]);
			fi
			
			if test x"$USE_UBSAN_GCC6" == x"yes" ; then

				AC_MSG_RESULT(["Enabling More Gcc-UbSan (gcc6)"]);
			fi
		fi
		#--
	
	fi

else 
	echo x"$enable_debug_checks"
	echo x"no"
	
	if test x"$enable_debug_checks" != x"no" ;
	then
		AC_MSG_ERROR(["Well something isnt right, debug checks should be enabled or disabled (yes/no), was $enable_debug_checks"])
	fi
fi

if test x"$enable_experimental_cpp11" == x"yes" ;
then
	#Should work for gcc/clang
	CXXFLAGS="$CXXFLAGS --std=c++11"
fi


if test x"$CXX" = xdistcc ; then
	AC_MSG_RESULT(["Adding distcc link flags"]);
	#Add -lstdc++ to libs for distcc
	LIBS="$LIBS -lstdc++"
fi

use_openvdb="yes"
if test x"$use_openvdb" = x"yes" ;
then
         #Add openvdb link flags
         LIBS="$LIBS -lopenvdb -ltbb -lHalf"
fi
LDFLAGS="$LDFLAGS $VDB_LIBS"


# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_SIZE_T 
AC_TYPE_UINT64_T 

# Checks for library functions.
AC_CHECK_FUNCS([atexit sqrt])
AC_CHECK_FUNCS([floor getcwd gettimeofday isascii memset ])
AC_CHECK_FUNCS([pow setlocale strdup sysinfo])
AC_C_INLINE()

#check for some headers
AC_CHECK_HEADERS([fenv.h libintl.h sys/time.h])
AC_CHECK_TYPES([ptrdiff_t])

#Check platform specific headers
case "${host_os}" in 
	darwin*)
		 AC_CHECK_HEADERS([mach/mach.h])
		;;
	*)
		;;
esac

AC_PROG_INSTALL
AC_OUTPUT

