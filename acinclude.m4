dnl CONFIG_INTERFACE(package,macro_name,interface_id,help,action-if-given,
dnl                  $1      $2         $3           $4   $5
dnl                  yes,dynamic,no)
dnl                  $6  $7      $8
AC_DEFUN(CONFIG_INTERFACE,
[AC_ARG_ENABLE($1,[$4],[$5])
case "x$enable_$1" in
xyes)
  AM_CONDITIONAL(ENABLE_$2, true)
  EXTRADEFS="$EXTRADEFS -DIA_$2"
  $6
  ;;
xdynamic)
  dynamic_targets="$dynamic_targets interface_$3.\$(so)"
  $7
  ;;
*)
  $8
  ;;
esac
AC_SUBST($3_so_libs)
])


dnl alsa.m4 starts form here
dnl Configure Paths for Alsa
dnl Christopher Lansdown (lansdoct@cs.alfred.edu)
dnl 29/10/1998
dnl modified for TiMidity++ by Isaku Yamahata(yamahata@kusm.kyoto-u.ac.jp)
dnl 16/12/1998
dnl AM_PATH_ALSA_LOCAL(MINIMUM-VERSION)
dnl Test for libasound, and define ALSA_CFLAGS and ALSA_LIBS as appropriate.
dnl if there exit ALSA, define have_alsa=yes, otherwise no.
dnl enables arguments --with-alsa-prefix= --with-alsa-enc-prefix= --disable-alsatest
dnl
AC_DEFUN(AM_PATH_ALSA_LOCAL,
[dnl
dnl Get the clfags and libraries for alsa
dnl
have_alsa=no
AC_ARG_WITH(alsa-prefix,[  --with-alsa-prefix=PFX  Prefix where Alsa library is installed(optional)],
	[alsa_prefix="$withval"], [alsa_prefix=""])
AC_ARG_WITH(alsa-inc-prefix, [  --with-alsa-inc-prefix=PFX  Prefix where include libraries are (optional)],
	[alsa_inc_prefix="$withval"], [alsa_inc_prefix=""])
AC_ARG_ENABLE(alsatest, [  --disable-alsatest      Do not try to compile and run a test Alsa program], [enable_alsatest=no], [enable_alsatest=yes])

dnl Add any special include directories
AC_MSG_CHECKING(for ALSA CFLAGS)
if test "$alsa_inc_prefix" != "" ; then
	ALSA_CFLAGS="$ALSA_CFLAGS -I$alsa_inc_prefix"
        CFLAGS="$CFLAGS -I$alsa_inc_prefix"
fi
AC_MSG_RESULT($ALSA_CFLAGS)

dnl add any special lib dirs
AC_MSG_CHECKING(for ALSA LDFLAGS)
if test "$alsa_prefix" != "" ; then
	ALSA_LIBS="$ALSA_LIBS -L$alsa_prefix"
	LIBS="-L$alsa_prefix"
fi

dnl add the alsa library
ALSA_LIBS="$ALSA_LIBS -lasound"
AC_MSG_RESULT($ALSA_LIBS)

dnl Check for the presence of the library
dnl if test $enable_alsatest = yes; then
dnl   AC_MSG_CHECKING(for working libasound)
dnl   AC_TRY_RUN([
dnl #include <sys/asoundlib.h>
dnl void main(void)
dnl {
dnl   snd_cards();
dnl   exit(0);
dnl }
dnl ],
dnl    [AC_MSG_RESULT("present")],
dnl    [AC_MSG_RESULT("not found. ")
dnl    AC_MSG_ERROR(Fatal error: Install alsa-lib package or use --with-alsa-prefix option...)],
dnl    [AC_MSG_RESULT(unsopported)
dnl     AC_MSG_ERROR(Cross-compiling isn't supported...)]
dnl  )
dnl fi

dnl Check for a working version of libasound that is of the right version.
min_alsa_version=ifelse([$1], ,0.1.1,$1)
AC_MSG_CHECKING(for libasound headers version >= $min_alsa_version)
no_alsa=""
    alsa_min_major_version=`echo $min_alsa_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    alsa_min_minor_version=`echo $min_alsa_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    alsa_min_micro_version=`echo $min_alsa_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`

AC_TRY_COMPILE([
#include <sys/asoundlib.h>
], [
/* ensure backward compatibility */
#if !defined(SND_LIB_MAJOR) && defined(SOUNDLIB_VERSION_MAJOR)
#define SND_LIB_MAJOR SOUNDLIB_VERSION_MAJOR
#endif
#if !defined(SND_LIB_MINOR) && defined(SOUNDLIB_VERSION_MINOR)
#define SND_LIB_MINOR SOUNDLIB_VERSION_MINOR
#endif
#if !defined(SND_LIB_SUBMINOR) && defined(SOUNDLIB_VERSION_SUBMINOR)
#define SND_LIB_SUBMINOR SOUNDLIB_VERSION_SUBMINOR
#endif

#  if(SND_LIB_MAJOR > $alsa_min_major_version)
  exit(0);
#  else
#    if(SND_LIB_MAJOR < $alsa_min_major_version)
#       error not present
#    endif

#   if(SND_LIB_MINOR > $alsa_min_minor_version)
  exit(0);
#   else
#     if(SND_LIB_MINOR < $alsa_min_minor_version)
#          error not present
#      endif

#      if(SND_LIB_SUBMINOR < $alsa_min_micro_version)
#        error not present
#      endif
#    endif
#  endif
exit(0);
],
  [AC_MSG_RESULT(found.)
   have_alsa=yes],
  [AC_MSG_RESULT(not present.)]
)

dnl Now that we know that we have the right version, let's see if we have the library and not just the headers.
AC_CHECK_LIB([asound], [snd_cards],,
	[AC_MSG_RESULT(No linkable libasound was found.)]
)

dnl That should be it.  Now just export out symbols:
AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)
])
dnl alsa.m4 ends here


dnl CHECK_DLSYM_UNDERSCORE([ACTION-IF-NEED [, ACTION IF-NOT-NEED]])
AC_DEFUN(CHECK_DLSYM_UNDERSCORE,
[dnl Check if dlsym need a leading underscore
AC_MSG_CHECKING(whether your dlsym() needs a leading underscore)
AC_CACHE_VAL(timidity_cv_func_dlsym_underscore,
[case "$ac_cv_header_dlfcn_h" in
yes) i_dlfcn=define;;
*)   i_dlfcn=undef;;
esac
cat > dyna.c <<EOM
fred () { }
EOM

cat > fred.c <<EOM
#include <stdio.h>
#$i_dlfcn I_DLFCN
#ifdef I_DLFCN
#include <dlfcn.h>      /* the dynamic linker include file for Sunos/Solaris */
#else
#include <sys/types.h>
#include <nlist.h>
#include <link.h>
#endif

extern int fred() ;

main()
{
    void * handle ;
    void * symbol ;
#ifndef RTLD_LAZY
    int mode = 1 ;
#else
    int mode = RTLD_LAZY ;
#endif
    handle = dlopen("./dyna.$so", mode) ;
    if (handle == NULL) {
	printf ("1\n") ;
	fflush (stdout) ;
	exit(0);
    }
    symbol = dlsym(handle, "fred") ;
    if (symbol == NULL) {
	/* try putting a leading underscore */
	symbol = dlsym(handle, "_fred") ;
	if (symbol == NULL) {
	    printf ("2\n") ;
	    fflush (stdout) ;
	    exit(0);
	}
	printf ("3\n") ;
    }
    else
	printf ("4\n") ;
    fflush (stdout) ;
    exit(0);
}
EOM
: Call the object file tmp-dyna.o in case dlext=o.
if ${CC-cc} $CFLAGS $SHCFLAGS $CPPFLAGS -c dyna.c > /dev/null 2>&1 &&
	mv dyna.o tmp-dyna.o > /dev/null 2>&1 && 
	$SHLD $SHLDFLAGS -o dyna.$so tmp-dyna.o > /dev/null 2>&1 && 
	${CC-cc} -o fred $CFLAGS $CPPFLAGS $LDFLAGS fred.$ac_ext $LIBS > /dev/null 2>&1; then
	xxx=`./fred`
	case $xxx in
	1)	AC_MSG_WARN(Test program failed using dlopen.  Perhaps you should not use dynamic loading.)
		;;
	2)	AC_MSG_WARN(Test program failed using dlsym.  Perhaps you should not use dynamic loading.)
		;;
	3)	timidity_cv_func_dlsym_underscore=yes
		;;
	4)	timidity_cv_func_dlsym_underscore=no
		;;
	esac
else
	AC_MSG_WARN(I can't compile and run the test program.)
fi
rm -f dyna.c dyna.o dyna.$so tmp-dyna.o fred.c fred.o fred
])
case "x$timidity_cv_func_dlsym_underscore" in
xyes)	[$1]
	AC_MSG_RESULT(yes)
	;;
xno)	[$2]
	AC_MSG_RESULT(no)
	;;
esac
])


dnl SET_UNIQ_SORT_WORD(shell-variable,words...)
AC_DEFUN(SET_UNIQ_SORT_WORD,
[$1=`for f in $2; do echo $f; done | sort | uniq`
$1=`echo $$1`
])
