AC_INIT([3Depict], [0.0.2], [mycae@yahoo.com])
AM_INIT_AUTOMAKE([foreign])
AC_PROG_CXX
AC_PROG_CC
AM_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile src/Makefile ])
AM_CONDITIONAL(HAVE_WINDRES, which windres > /dev/null)

# Support {host_os} variable
AC_CANONICAL_HOST

dnl Test for wx-widgets
dnl ----------
AM_OPTIONS_WXCONFIG

AM_PATH_WXCONFIG(2.5.3, wxWin=1)

if test "$wxWin" != 1; then
AC_MSG_ERROR([
		wxWidgets must be installed on your system
		but wx-config script couldnt be found.

		Please check that wx-config is in path, the directory
		where wxWidgets libraries are installed (returned by
		'wx-config --libs' command) is in LD_LIBRARY_PATH or
		equivalent variable and wxWidgets version is 2.3.4 or above.
])
fi

#Append the --gl-libs flag
WX_LIBS="$WX_LIBS `$WX_CONFIG_PATH --gl-libs`"
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
#AX_CHECK_FTGL() #Not doing anything???

#AC_SUBST(FTGL_CFLAGS)
#AC_SUBST(FTGL_LIBS)

#AC_MSG_RESULT([FTGL_LIBS Set to be $FTGL_LIBS])
#AC_MSG_ERROR([Aborting FTGL_LIBS is $FTGL_LIBS])
FTGL_LIBS="-lftgl"
AC_SUBST(FTGL_LIBS)
dnl ----------


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
else
    PKG_CHECK_MODULES(PNG, libpng >= 1.2)
fi
AC_SUBST(PNG_CFLAGS)

#set libpng's link flags
if test x"$with_libpng_link" != x"" ;
then
    PNG_LIBS="$with_libpng_link"
else
    PKG_CHECK_MODULES(PNG, libpng >= 1.2)
fi

AC_SUBST(PNG_LIBS)

#Attempt to compile a test program
CFLAGS_ORIG="$CFLAGS"
CFLAGS="$CFLAGS $PNG_CFLAGS"
AC_CHECK_HEADER([png.h],[AC_DEFINE(HAVE_PNG,[],[Have got libpng headers])],
	[AC_MSG_ERROR([Required libpng headers not found (looking for png.h])])

LIBS_ORIG="$LIBS"
LIBS="$LIBS $PNG_LIBS"
AC_CHECK_LIB(png, png_sig_cmp, [AC_DEFINE(HAVE_PNG,[],[PNG compilation OK])] , AC_MSG_ERROR([You must have libpng installed and be able to compile sample program]), -lz -lm)
CFLAGS="$CFLAGS_ORIG"
LIBS="$LIBS_ORIG"
	
dnl--------------


#Check for opengl
#------------

AC_MSG_CHECKING([for GLU library])

#Try linking against gluSphere windows usese -lglu32; mac?? ; linux -lGLU
case "${host_os}" in 
	mingw*|windows*|winnt)
		AC_CHECK_LIB(GLU, gluSphere, GL_LIBS="$GLU_LIBS -lglu", 
			     [AC_MSG_ERROR([Could not find GLU library])])
		GL_LIBS="-lopengl32 -lglu32"
	    ;;
	darwin*)
		GL_FLAGS="-framework GL"
	    ;;
	 *)
		AC_CHECK_LIB(GLU, gluSphere, GL_LIBS="$GL_LIBS -lglu", [AC_MSG_ERROR([Could not find GLU library])])
		GL_LIBS="-lGL -lGLU"

	   ;;
esac

AC_SUBST(GL_LIBS)

#------------

#Check for Mathgl
dnl ----------
AC_CHECK_HEADER([mgl/mgl_c.h],[AC_DEFINE(HAVE_MGL_H,[],[Have got mathgl headers])],
	[AC_MSG_ERROR([Required MathGL headers not found (looking for mgl/mg_c.h])])

AC_SUBST(MGL_LIBS,[-lmgl])


dnl -----------

# Check for hpmc isosurface shader library  enable
#http://www.sintef.no/Projectweb/Heterogeneous-Computing/
# Research-Topics/Marching-Cubes-using-Histogram-Pyramids/
#----------

#AC_CHECK_HEADER([hpmc/hpmc.h],[AC_DEFINE(HPMC_GPU_ISOSURFACE,[],[Have HPMC isosurface headers. using])],
#	[AC_MSG_WARN([=================================Unable to find GPU isosurface shader library HPMC -- will use much slower CPU method=================])])

#-----------

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST

# Checks for library functions.
AC_CHECK_FUNCS([atexit])
AC_CHECK_FUNCS([sqrt])
AC_C_INLINE()

AC_PROG_INSTALL
AC_OUTPUT
