dnl Process this file with autoconf to produce a configure script.


AC_INIT(c_icap_classify,00000x)

AM_MAINTAINER_MODE
AM_CONFIG_HEADER(autoconf.h)

AM_INIT_AUTOMAKE([subdir-objects])

AC_PROG_CC
AM_PROG_CC_C_O

AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

dnl AC_CANONICAL_HOST

CFLAGS="$CFLAGS -Wall"

dnl check for c-icap
AC_ARG_WITH(c-icap,
[  --with-c-icap	Where to find c-icap ],
[
    cicapdir="$withval"
    if test -f $withval/bin/c-icap-config; then
       cicapflags=`$withval/bin/c-icap-config --cflags`
       cicaplibs=`$withval/bin/c-icap-config --libs`
       cicapetc=`$withval/bin/c-icap-config --configdir`
       cicapprefix=`$withval/bin/c-icap-config --prefix`
       cicapmods=`$withval/bin/c-icap-config --modulesdir`
       cicapdatadir=`$withval/bin/c-icap-config --datarootdir`
    else
       cicapflags=`c-icap-config --cflags`
       cicaplibs=`c-icap-config --libs`
       cicapetc=`c-icap-config --configdir`
       cicapprefix=`c-icap-config --prefix`
       cicapmods=`c-icap-config --modulesdir`
       cicapdatadir=`c-icap-config --datarootdir`
    fi
],
)

if test -z "$cicapdir"; then
    if c-icap-config >& /dev/null; then
        cicapflags=`c-icap-config --cflags`
        cicaplibs=`c-icap-config --libs`
        cicapetc=`c-icap-config --configdir`
        cicapprefix=`c-icap-config --prefix`
        cicapmods=`c-icap-config --modulesdir`
        cicapdatadir=`c-icap-config --datarootdir`
    else
        echo "ERROR: c-icap-config not found!"
	exit -1;
    fi
fi

AM_CONDITIONAL(CICAPMODULESDIR, [test -n $cicapmods])

CFLAGS="$CFLAGS $cicapflags"
MODULES_LIBADD="$cicaplibs"
CONFDIR="$cicapetc"
MODSDIR="$cicapmods"
DATADIR="$cicapdatadir"
AC_SUBST(MODULES_LIBADD)
AC_SUBST(CONFDIR)
AC_SUBST(MODSDIR)
AC_SUBST(DATADIR)

dnl Checks for libraries

dnl Check for zlib
AC_ARG_WITH(zlib,
[	--with-zlib	Path to zlib library],
[
case "$withval" in
     yes)
	zlib=yes;
     ;;
     no)
	zlib=no;
     ;;
     *)
	zlib=$withval;
     ;;
     esac
],
[ zlib=yes]
)

if test a"$zlib" != "ano"; then
   if test a"$zlib" != "ayes"; then
        OLDCFLAGS=$CFLAGS
        OLDLDFLAGS=$LDFLAGS
        CFLAGS="$CFLAGS -I$zlib/include"
        LDFLAGS="$LDFLAGS -L $zlib/lib"
	AC_CHECK_LIB(z,inflate,[ZLIB_ADD_LDFLAG="-L$zlib/lib -lz";ZLIB_ADD_FLAG="-I$zlib/include";AC_DEFINE(HAVE_ZLIB,1,[Define HAVE_ZLIB if zlib installed])],[zlib=no])
        CFLAGS=$OLDCFLAGS
        LDFLAGS=$OLDLDFLAGS
	AC_SUBST(ZLIB_ADD_LDFLAG)
	AC_SUBST(ZLIB_ADD_FLAG)
   else
	AC_CHECK_LIB(z,inflate,[ZLIB_ADD_LDFLAG=" -lz";AC_DEFINE(HAVE_ZLIB,1,[Define HAVE_ZLIB if zlib installed])],[zlib=no])
	AC_SUBST(ZLIB_ADD_LDFLAG)
   fi
fi
if test a"$zlib" = "ano"; then
    AC_MSG_WARN("zlib is most likely required for srv_classify to work with most browsers!!")
fi

dnl Determine whether or not we are big or little endian
AC_MSG_CHECKING([for endianness])
AC_TRY_RUN(
[
int main()
{
short one = 1;
char *cp = (char*)&one;
if ( *cp == 0 )
return(0);
else
return(1);
}
], [echo big-endian; CFLAGS="$CFLAGS -DBIG_ENDIAN"; AC_DEFINE(BIG_ENDIAN, 1, [Define BIG_ENDIAN])],[echo little-endian; CFLAGS="$CFLAGS -DLITTLE_ENDIAN"; AC_DEFINE(LITTLE_ENDIAN, 1, [Define LITTLE_ENDIAN])],[echo cross-compiling])

dnl Check for OpenCV
AC_ARG_WITH(opencv,
[  --with-opencv       Where to find opencv ],
[ case "$withval" in
  yes)
#    AC_MSG_WARN([--with-opencv called without argument])
    opencv="yes"
  ;;
  no )
    opencv="no"
  ;;
  * )
    opencv="yes"
    opencvinc="-I$withval/include/opencv"
    opencvlib="-L$withval/lib"
  ;;
  esac
],
)

AC_SUBST(opencvinc)
AC_SUBST(opencvlib)
OLD_CFLAGS=$CFLAGS
OLD_LIBS=$LIBS
LIBS=$opencvlib
CFLAGS=$opencvinc
opencv="no";
dnl -------------------------------------------------------------
AC_MSG_CHECKING([for OpenCV older than 2.2.x])
AC_CHECK_LIB(cxcore, cvCreateImage, [OPENCV_ADD_LDFLAG="-lcv -lcxcore -lhighgui";  opencv="yes"; AC_DEFINE(HAVE_OPENCV, 1, [Define HAVE_OPENCV if OpenCV installed])])
AC_MSG_CHECKING([for OpenCV 2.2.x or newer])
AC_CHECK_LIB(opencv_core, cvCreateImage, [OPENCV_ADD_LDFLAG="-lopencv_core -lopencv_highgui -lopencv_objdetect -lopencv_features2d";  opencv="yes"; AC_DEFINE(HAVE_OPENCV_22X, 1, [Define HAVE_OPENCV if OpenCV installed])])
AC_MSG_CHECKING([for OpenCV 2.3.x or newer])
AC_CHECK_LIB(opencv_imgproc, cvResize, [OPENCV_ADD_LDFLAG="-lopencv_core -lopencv_highgui -lopencv_objdetect -lopencv_features2d -lopencv_imgproc";  opencv="yes"; AC_DEFINE(HAVE_OPENCV_23X, 1, [Define HAVE_OPENCV if 2.3.x or newer OpenCV installed])])
AC_MSG_CHECKING([for OpenCV 3.0 or newer])
AC_CHECK_LIB(opencv_core, cvRound, [], [echo "Disabling support for OpenCV >= 3.0"; OPENCV_ADD_CFLAG=""; OPENCV_ADD_LDFLAG=""; opencv="no"; AC_DEFINE(HAVE_OPENCV_30X, 1, [Don't Define HAVE_OPENCV_30X if 3.0.x or newer OpenCV installed])])
AC_SUBST(OPENCV_ADD_LDFLAG)
AC_SUBST(OPENCV_ADD_CFLAG)
CFLAGS=$OLD_CFLAGS
LIBS=$OLD_LIBS

if test "$opencv" = "no" ; then
	AC_SUBST(opencvinc)
	AC_SUBST(opencvlib)
        unset ac_cv_lib_cxcore_cvCreateImage
        unset ac_cv_lib_opencv_core_cvCreateImage
        unset ac_cv_lib_opencv_imgproc_cvResize
	OLD_CFLAGS=$CFLAGS
	OLD_LIBS=$LIBS
	LIBS=$opencvlib
	CFLAGS="$opencvinc -fPIC"
	opencv="no";
	dnl -------------------------------------------------------------
	AC_MSG_CHECKING([for OpenCV older than 2.2.x w/ -fPIC])
	AC_CHECK_LIB(cxcore, cvCreateImage, [OPENCV_ADD_CFLAG="-fPIC"; OPENCV_ADD_LDFLAG="-lcv -lcxcore -lhighgui"; opencv="yes"; AC_DEFINE(HAVE_OPENCV, 1, [Define HAVE_OPENCV if OpenCV installed])])
	AC_MSG_CHECKING([for OpenCV 2.2.x or newer w/ -fPIC])
	AC_CHECK_LIB(opencv_core, cvCreateImage, [OPENCV_ADD_CFLAG="-fPIC"; OPENCV_ADD_LDFLAG="-lopencv_core -lopencv_highgui -lopencv_objdetect -lopencv_features2d"; opencv="yes"; AC_DEFINE(HAVE_OPENCV_22X, 1, [Define HAVE_OPENCV if OpenCV installed])])
	AC_MSG_CHECKING([for OpenCV 2.3.x or newer w/ -fPIC])
	AC_CHECK_LIB(opencv_imgproc, cvResize, [OPENCV_ADD_CFLAG="-fPIC"; OPENCV_ADD_LDFLAG="-lopencv_core -lopencv_highgui -lopencv_objdetect -lopencv_features2d -lopencv_imgproc"; opencv="yes"; AC_DEFINE(HAVE_OPENCV_23X, 1, [Define HAVE_OPENCV if 2.3.x or newer OpenCV installed])])
	AC_MSG_CHECKING([for OpenCV 3.0 or newer])
	AC_CHECK_LIB(opencv_core, cvRound, [], [echo "Disabling support for OpenCV >= 3.0"; OPENCV_ADD_CFLAG=""; OPENCV_ADD_LDFLAG=""; opencv="no"; AC_DEFINE(HAVE_OPENCV_30X, 1, [Don't Define HAVE_OPENCV_30X if 3.0.x or newer OpenCV installed])])
	AC_SUBST(OPENCV_ADD_LDFLAG)
	AC_SUBST(OPENCV_ADD_CFLAG)
	CFLAGS=$OLD_CFLAGS
	LIBS=$OLD_LIBS
fi

dnl Check for TRE Regex Library
dnl -------------------------------------------------------------
AC_MSG_CHECKING([for TRE regex header (tre/tre.h)])
AC_CHECK_HEADER(tre/tre.h, [AC_DEFINE(HAVE_TRE_HEADER, 1, [Define HAVE_TRE_HEADER if TRE header found])], [AC_MSG_ERROR([Sorry, tre/tre.h not found, this is required])])
AC_MSG_CHECKING([for TRE regex library])
AC_CHECK_LIB(tre, tre_regwncomp, [AC_DEFINE(HAVE_TRE_LIB, 1, [Define HAVE_TRE_LIB if TRE library found])], [AC_MSG_ERROR([Sorry, TRE regex library not found, this is required])])

dnl Check for scandir
dnl -------------------------------------------------------------
AC_CHECK_FUNCS(scandir, [AC_DEFINE(_SVID_SOURCE, 1, [Define _SVID_SOURCE if scandir function is found])])

dnl Check for libicu
dnl -------------------------------------------------------------
AC_DEFUN([AC_CHECK_ICU], [
  succeeded=no

  if test -z "$ICU_CONFIG"; then
    AC_PATH_PROG(ICU_CONFIG, icu-config, no)
  fi

  if test "$ICU_CONFIG" = "no" ; then
    echo "*** The icu-config script could not be found. Make sure it is"
    echo "*** in your path, and that taglib is properly installed."
    echo "*** Or see http://ibm.com/software/globalization/icu/"
  else
    ICU_VERSION=`$ICU_CONFIG --version`
    AC_MSG_CHECKING(for ICU >= $1)
        VERSION_CHECK=`expr $ICU_VERSION \>\= $1`
        if test "$VERSION_CHECK" = "1" ; then
            AC_MSG_RESULT(yes)
            succeeded=yes

            AC_MSG_CHECKING(ICU_CFLAGS)
            ICU_CFLAGS=`$ICU_CONFIG --cflags`
            AC_MSG_RESULT($ICU_CFLAGS)

            AC_MSG_CHECKING(ICU_CXXFLAGS)
            ICU_CXXFLAGS=`$ICU_CONFIG --cxxflags`
            AC_MSG_RESULT($ICU_CXXFLAGS)

            AC_MSG_CHECKING(ICU_LIBS)
            ICU_LIBS=`$ICU_CONFIG --ldflags`
            AC_MSG_RESULT($ICU_LIBS)

            AC_DEFINE(HAVE_LIBICU, 1, [Define HAVE_LIBICU if libicu is installed])
        else
            ICU_CFLAGS=""
            ICU_CXXFLAGS=""
            ICU_LIBS=""
            ## If we have a custom action on failure, don't print errors, but
            ## do set a variable so people can do so.
            ifelse([$3], ,echo "can't find ICU &gt;= $1",)
        fi

        AC_SUBST(ICU_CFLAGS)
        AC_SUBST(ICU_CXXFLAGS)
        AC_SUBST(ICU_LIBS)
  fi

  if test $succeeded = yes; then
     ifelse([$2], , :, [$2])
  else
     ifelse([$3], , AC_MSG_ERROR([Library requirements (ICU) not met.]), [$3])
  fi
])

AC_MSG_CHECKING([for libicu])
AC_CHECK_ICU(50.1.2)

dnl Check for programs
AC_PROG_LN_S
AC_PATH_PROG(LN, ln, cp)
AC_PATH_PROG(RM, rm, $FALSE)
dnl Libtool 2.2.6 requires: rm -f
RM="$RM -f"

dnl Now determine which modules will going to build .....

AM_CONDITIONAL(USEOPENCV,[test a"$opencv" != ano])

AC_OUTPUT([Makefile \
services/Makefile \
services/classify/Makefile \
docs/Makefile docs/man/Makefile \
])
