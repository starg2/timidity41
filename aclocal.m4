dnl aclocal.m4 generated automatically by aclocal 1.4

dnl Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY, to the extent permitted by law; without
dnl even the implied warranty of MERCHANTABILITY or FITNESS FOR A
dnl PARTICULAR PURPOSE.

dnl MY_DEFINE(VARIABLE)
AC_DEFUN(MY_DEFINE,
[cat >> confdefs.h <<EOF
[#define] $1 1
EOF
])

dnl CONFIG_INTERFACE(package,macro_name,interface_id,help
dnl                  $1      $2         $3           $4
dnl                  action-if-yes-or-dynamic,
dnl		     $5
dnl		     action-if-yes,action-if-dynamic,action-if-no)
dnl		     $6            $7                $8
AC_DEFUN(CONFIG_INTERFACE,
[AC_ARG_ENABLE($1,[$4],
[case "x$enable_$1" in xyes|xdynamic) $5 ;; esac])
case "x$enable_$1" in
xyes)
  MY_DEFINE(IA_$2)
  AM_CONDITIONAL(ENABLE_$2, true)
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
dnl   KEEP_LDFLAGS="$LDFLAGS"
dnl   LDFLAGS="$LDFLAGS $ALSA_LIBS"
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
dnl   LDFLAGS="$KEEP_LDFLAGS"
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

# Configure paths for ESD
# Manish Singh    98-9-30
# stolen back from Frank Belew
# stolen from Manish Singh
# Shamelessly stolen from Owen Taylor

dnl AM_PATH_ESD([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for ESD, and define ESD_CFLAGS and ESD_LIBS
dnl
AC_DEFUN(AM_PATH_ESD,
[dnl 
dnl Get the cflags and libraries from the esd-config script
dnl
AC_ARG_WITH(esd-prefix,[  --with-esd-prefix=PFX   Prefix where ESD is installed (optional)],
            esd_prefix="$withval", esd_prefix="")
AC_ARG_WITH(esd-exec-prefix,[  --with-esd-exec-prefix=PFX Exec prefix where ESD is installed (optional)],
            esd_exec_prefix="$withval", esd_exec_prefix="")
AC_ARG_ENABLE(esdtest, [  --disable-esdtest       Do not try to compile and run a test ESD program],
		    , enable_esdtest=yes)

  if test x$esd_exec_prefix != x ; then
     esd_args="$esd_args --exec-prefix=$esd_exec_prefix"
     if test x${ESD_CONFIG+set} != xset ; then
        ESD_CONFIG=$esd_exec_prefix/bin/esd-config
     fi
  fi
  if test x$esd_prefix != x ; then
     esd_args="$esd_args --prefix=$esd_prefix"
     if test x${ESD_CONFIG+set} != xset ; then
        ESD_CONFIG=$esd_prefix/bin/esd-config
     fi
  fi

  AC_PATH_PROG(ESD_CONFIG, esd-config, no)
  min_esd_version=ifelse([$1], ,0.2.7,$1)
  AC_MSG_CHECKING(for ESD - version >= $min_esd_version)
  no_esd=""
  if test "$ESD_CONFIG" = "no" ; then
    no_esd=yes
  else
    ESD_CFLAGS=`$ESD_CONFIG $esdconf_args --cflags`
    ESD_LIBS=`$ESD_CONFIG $esdconf_args --libs`

    esd_major_version=`$ESD_CONFIG $esd_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    esd_minor_version=`$ESD_CONFIG $esd_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    esd_micro_version=`$ESD_CONFIG $esd_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_esdtest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $ESD_CFLAGS"
      LIBS="$LIBS $ESD_LIBS"
dnl
dnl Now check if the installed ESD is sufficiently new. (Also sanity
dnl checks the results of esd-config to some extent
dnl
      rm -f conf.esdtest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esd.h>

char*
my_strdup (char *str)
{
  char *new_str;
  
  if (str)
    {
      new_str = malloc ((strlen (str) + 1) * sizeof(char));
      strcpy (new_str, str);
    }
  else
    new_str = NULL;
  
  return new_str;
}

int main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.esdtest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = my_strdup("$min_esd_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_esd_version");
     exit(1);
   }

   if (($esd_major_version > major) ||
      (($esd_major_version == major) && ($esd_minor_version > minor)) ||
      (($esd_major_version == major) && ($esd_minor_version == minor) && ($esd_micro_version >= micro)))
    {
      return 0;
    }
  else
    {
      printf("\n*** 'esd-config --version' returned %d.%d.%d, but the minimum version\n", $esd_major_version, $esd_minor_version, $esd_micro_version);
      printf("*** of ESD required is %d.%d.%d. If esd-config is correct, then it is\n", major, minor, micro);
      printf("*** best to upgrade to the required version.\n");
      printf("*** If esd-config was wrong, set the environment variable ESD_CONFIG\n");
      printf("*** to point to the correct copy of esd-config, and remove the file\n");
      printf("*** config.cache before re-running configure\n");
      return 1;
    }
}

],, no_esd=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_esd" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$ESD_CONFIG" = "no" ; then
       echo "*** The esd-config script installed by ESD could not be found"
       echo "*** If ESD was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the ESD_CONFIG environment variable to the"
       echo "*** full path to esd-config."
     else
       if test -f conf.esdtest ; then
        :
       else
          echo "*** Could not run ESD test program, checking why..."
          CFLAGS="$CFLAGS $ESD_CFLAGS"
          LIBS="$LIBS $ESD_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include <esd.h>
],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding ESD or finding the wrong"
          echo "*** version of ESD. If it is not finding ESD, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means ESD was incorrectly installed"
          echo "*** or that you have moved ESD since it was installed. In the latter case, you"
          echo "*** may want to edit the esd-config script: $ESD_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     ESD_CFLAGS=""
     ESD_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(ESD_CFLAGS)
  AC_SUBST(ESD_LIBS)
  rm -f conf.esdtest
])

dnl CHECK_DLSYM_UNDERSCORE([ACTION-IF-NEED [, ACTION IF-NOT-NEED]])
dnl variable input:
dnl   CC CFLAGS CPPFLAGS LDFLAGS LIBS SHCFLAGS SHLD SHLDFLAGS
dnl   ac_cv_header_dlfcn_h lib_dl_opt so
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
	${CC-cc} -o fred $CFLAGS $CPPFLAGS $LDFLAGS fred.c $LIBS $lib_dl_opt > /dev/null 2>&1; then
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


dnl contains program from perl5
dnl CONTAINS_INIT()
AC_DEFUN(CONTAINS_INIT,
[dnl Some greps do not return status, grrr.
AC_MSG_CHECKING(whether grep returns status)
echo "grimblepritz" >grimble
if grep blurfldyick grimble >/dev/null 2>&1 ; then
	contains="./contains"
elif grep grimblepritz grimble >/dev/null 2>&1 ; then
	contains=grep
else
	contains="./contains"
fi
rm -f grimble
dnl the following should work in any shell
case "$contains" in
grep)	AC_MSG_RESULT(yes)
	;;
./contains)
	AC_MSG_RESULT(AGH!  Grep doesn't return a status.  Attempting remedial action.)
	cat >./contains <<'EOSS'
grep "[$]1" "[$]2" >.greptmp && cat .greptmp && test -s .greptmp
EOSS
	chmod +x "./contains"
	;;
esac
])


dnl CONTAINS(word,filename,action-if-found,action-if-not-found)
AC_DEFUN(CONTAINS,
[if $contains "^[$1]"'[$]' $2 >/dev/null 2>&1; then
  [$3]
else
  [$4]
fi
])


dnl SET_UNIQ_WORDS(shell-variable,words...)
AC_DEFUN(SET_UNIQ_WORDS,
[rm -f wordtmp >/dev/null 2>&1
val=''
for f in $2; do
  CONTAINS([$f],wordtmp,:,[echo $f >>wordtmp; val="$val $f"])
done
$1="$val"
rm -f wordtmp >/dev/null 2>&1
])


dnl WAPI_CHECK_FUNC(FUNCTION, INCLUDES, TEST-BODY,
		    [ACTION-FI-FOUND [, ACTION-IF-NOT-FOUND]])
AC_DEFUN(WAPI_CHECK_FUNC,
[AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(wapi_cv_func_$1,
[AC_TRY_LINK([#include <windows.h>
$2
], [$3],
wapi_cv_func_$1=yes, wapi_cv_func_$1=no)])
if eval "test \"`echo '$wapi_cv_func_'$1`\" = yes"; then
  AC_MSG_RESULT(yes)
  ifelse([$4], , :, [$4])
else
  AC_MSG_RESULT(no)
ifelse([$5], , , [$5
])dnl
fi
])


dnl WAPI_CHECK_LIB(LIBRARY, FUNCTION,
dnl		INCLUDES, TEST-BODY
dnl		[, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND
dnl		[, OTHER-LIBRARIES]]])
AC_DEFUN(WAPI_CHECK_LIB,
[AC_MSG_CHECKING([for $2 in -l$1])
ac_lib_var=`echo $1['_']$2 | sed 'y%./+-%__p_%'`
AC_CACHE_VAL(wapi_cv_lib_$ac_lib_var,
[ac_save_LIBS="$LIBS"
LIBS="-l$1 $7 $LIBS"
AC_TRY_LINK([#include <windows.h>
$3
], [$4],
eval "wapi_cv_lib_$ac_lib_var=yes",
eval "wapi_cv_lib_$ac_lib_var=no")
LIBS="$ac_save_LIBS"
])dnl
if eval "test \"`echo '$wapi_cv_lib_'$ac_lib_var`\" = yes"; then
  AC_MSG_RESULT(yes)
  ifelse([$5], ,LIBS="-l$1 $LIBS", [$5])
else
  AC_MSG_RESULT(no)
ifelse([$6], , , [$6
])dnl
fi
])


dnl EXTRACT_CPPFLAGS(CPPFLAGS-to-append,others-to-append,FLAGS)
AC_DEFUN(EXTRACT_CPPFLAGS,
[for f in $3; do
    case ".$f" in
	.-I?*|.-D?*)	$1="[$]$1 $f" ;;
	*)		$2="[$]$1 $f" ;;
    esac
done
])


dnl CHECK_COMPILER_OPTION(OPTIONS [, ACTION-IF-SUCCEED [, ACTION-IF-FAILED]])
AC_DEFUN(CHECK_COMPILER_OPTION,
[AC_MSG_CHECKING([whether -$1 option is recognized])
ac_ccoption=`echo $1 | sed 'y%./+-%__p_%'`
AC_CACHE_VAL(timidity_cv_ccoption_$ac_ccoption,
[cat > conftest.$ac_ext <<EOF
int main() {return 0;}
EOF
if ${CC-cc} $LDFLAGS $CFLAGS -o conftest${ac_exeext} -$1 conftest.$ac_ext > conftest.out 2>&1; then
    if test -s conftest.out; then
	eval "timidity_cv_ccoption_$ac_ccoption=no"
    else
	eval "timidity_cv_ccoption_$ac_ccoption=yes"
    fi
else
    eval "timidity_cv_ccoption_$ac_ccoption=no"
fi
])
if eval "test \"`echo '$timidity_cv_ccoption_'$ac_ccoption`\" = yes"; then
  AC_MSG_RESULT(yes)
  ifelse([$2], , , [$2
])
else
  AC_MSG_RESULT(no)
ifelse([$3], , , [$3
])
fi
])


dnl MY_SEARCH_LIBS(FUNCTION, LIBRARIES [, ACTION-IF-FOUND
dnl            [, ACTION-IF-NOT-FOUND [, OTHER-LIBRARIES]]])
dnl Search for a library defining FUNC, if it's not already available.

AC_DEFUN(MY_SEARCH_LIBS,
[AC_CACHE_CHECK([for library containing $1], [timidity_cv_search_$1],
[ac_func_search_save_LIBS="$LIBS"
timidity_cv_search_$1="no"
for i in $2; do
  LIBS="$i $5 $ac_func_search_save_LIBS"
  AC_TRY_LINK_FUNC([$1], [timidity_cv_search_$1="$i"; break])
done
LIBS="$ac_func_search_save_LIBS"])
if test "$timidity_cv_search_$1" != "no"; then
  $3
else :
  $4
fi])


# beginning of gtk.m4
# Configure paths for GTK+
# Owen Taylor     97-11-3

dnl AM_PATH_GTK([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for GTK, and define GTK_CFLAGS and GTK_LIBS
dnl
AC_DEFUN(AM_PATH_GTK,
[dnl 
dnl Get the cflags and libraries from the gtk-config script
dnl
AC_ARG_WITH(gtk-prefix,[  --with-gtk-prefix=PFX   Prefix where GTK is installed (optional)],
            gtk_config_prefix="$withval", gtk_config_prefix="")
AC_ARG_WITH(gtk-exec-prefix,[  --with-gtk-exec-prefix=PFX Exec prefix where GTK is installed (optional)],
            gtk_config_exec_prefix="$withval", gtk_config_exec_prefix="")
AC_ARG_ENABLE(gtktest, [  --disable-gtktest       Do not try to compile and run a test GTK program],
		    , enable_gtktest=yes)

  if test x$gtk_config_exec_prefix != x ; then
     gtk_config_args="$gtk_config_args --exec-prefix=$gtk_config_exec_prefix"
     if test x${GTK_CONFIG+set} != xset ; then
        GTK_CONFIG=$gtk_config_exec_prefix/bin/gtk-config
     fi
  fi
  if test x$gtk_config_prefix != x ; then
     gtk_config_args="$gtk_config_args --prefix=$gtk_config_prefix"
     if test x${GTK_CONFIG+set} != xset ; then
        GTK_CONFIG=$gtk_config_prefix/bin/gtk-config
     fi
  fi

  AC_PATH_PROG(GTK_CONFIG, gtk-config, no)
  min_gtk_version=ifelse([$1], ,0.99.7,$1)
  AC_MSG_CHECKING(for GTK - version >= $min_gtk_version)
  no_gtk=""
  if test "$GTK_CONFIG" = "no" ; then
    no_gtk=yes
  else
    GTK_CFLAGS=`$GTK_CONFIG $gtk_config_args --cflags`
    GTK_LIBS=`$GTK_CONFIG $gtk_config_args --libs`
    gtk_config_major_version=`$GTK_CONFIG $gtk_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    gtk_config_minor_version=`$GTK_CONFIG $gtk_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    gtk_config_micro_version=`$GTK_CONFIG $gtk_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_gtktest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GTK_CFLAGS"
      LIBS="$LIBS $GTK_LIBS"
dnl
dnl Now check if the installed GTK is sufficiently new. (Also sanity
dnl checks the results of gtk-config to some extent
dnl
      rm -f conf.gtktest
      AC_TRY_RUN([
#include <gtk/gtk.h>
#include <stdio.h>

int 
main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.gtktest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = g_strdup("$min_gtk_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_gtk_version");
     exit(1);
   }

  if ((gtk_major_version != $gtk_config_major_version) ||
      (gtk_minor_version != $gtk_config_minor_version) ||
      (gtk_micro_version != $gtk_config_micro_version))
    {
      printf("\n*** 'gtk-config --version' returned %d.%d.%d, but GTK+ (%d.%d.%d)\n", 
             $gtk_config_major_version, $gtk_config_minor_version, $gtk_config_micro_version,
             gtk_major_version, gtk_minor_version, gtk_micro_version);
      printf ("*** was found! If gtk-config was correct, then it is best\n");
      printf ("*** to remove the old version of GTK+. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If gtk-config was wrong, set the environment variable GTK_CONFIG\n");
      printf("*** to point to the correct copy of gtk-config, and remove the file config.cache\n");
      printf("*** before re-running configure\n");
    } 
  else
    {
      if ((gtk_major_version > major) ||
        ((gtk_major_version == major) && (gtk_minor_version > minor)) ||
        ((gtk_major_version == major) && (gtk_minor_version == minor) && (gtk_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of GTK+ (%d.%d.%d) was found.\n",
               gtk_major_version, gtk_minor_version, gtk_micro_version);
        printf("*** You need a version of GTK+ newer than %d.%d.%d. The latest version of\n",
	       major, minor, micro);
        printf("*** GTK+ is always available from ftp://ftp.gtk.org.\n");
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the gtk-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of GTK+, but you can also set the GTK_CONFIG environment to point to the\n");
        printf("*** correct copy of gtk-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
    }
  return 1;
}
],, no_gtk=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_gtk" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$GTK_CONFIG" = "no" ; then
       echo "*** The gtk-config script installed by GTK could not be found"
       echo "*** If GTK was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GTK_CONFIG environment variable to the"
       echo "*** full path to gtk-config."
     else
       if test -f conf.gtktest ; then
        :
       else
          echo "*** Could not run GTK test program, checking why..."
          CFLAGS="$CFLAGS $GTK_CFLAGS"
          LIBS="$LIBS $GTK_LIBS"
          AC_TRY_LINK([
#include <gtk/gtk.h>
#include <stdio.h>
],      [ return ((gtk_major_version) || (gtk_minor_version) || (gtk_micro_version)); ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GTK or finding the wrong"
          echo "*** version of GTK. If it is not finding GTK, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"
          echo "***"
          echo "*** If you have a RedHat 5.0 system, you should remove the GTK package that"
          echo "*** came with the system with the command"
          echo "***"
          echo "***    rpm --erase --nodeps gtk gtk-devel" ],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GTK was incorrectly installed"
          echo "*** or that you have moved GTK since it was installed. In the latter case, you"
          echo "*** may want to edit the gtk-config script: $GTK_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     GTK_CFLAGS=""
     GTK_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GTK_CFLAGS)
  AC_SUBST(GTK_LIBS)
  rm -f conf.gtktest
])

# end of gtk.m4

# Define a conditional.

AC_DEFUN(AM_CONDITIONAL,
[AC_SUBST($1_TRUE)
AC_SUBST($1_FALSE)
if $2; then
  $1_TRUE=
  $1_FALSE='#'
else
  $1_TRUE='#'
  $1_FALSE=
fi])

# Do all the work for Automake.  This macro actually does too much --
# some checks are only needed if your package does certain things.
# But this isn't really a big deal.

# serial 1

dnl Usage:
dnl AM_INIT_AUTOMAKE(package,version, [no-define])

AC_DEFUN(AM_INIT_AUTOMAKE,
[AC_REQUIRE([AC_PROG_INSTALL])
PACKAGE=[$1]
AC_SUBST(PACKAGE)
VERSION=[$2]
AC_SUBST(VERSION)
dnl test to see if srcdir already configured
if test "`cd $srcdir && pwd`" != "`pwd`" && test -f $srcdir/config.status; then
  AC_MSG_ERROR([source directory already configured; run "make distclean" there first])
fi
ifelse([$3],,
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Name of package])
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Version number of package]))
AC_REQUIRE([AM_SANITY_CHECK])
AC_REQUIRE([AC_ARG_PROGRAM])
dnl FIXME This is truly gross.
missing_dir=`cd $ac_aux_dir && pwd`
AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(AUTOMAKE, automake, $missing_dir)
AM_MISSING_PROG(AUTOHEADER, autoheader, $missing_dir)
AM_MISSING_PROG(MAKEINFO, makeinfo, $missing_dir)
AC_REQUIRE([AC_PROG_MAKE_SET])])

#
# Check to make sure that the build environment is sane.
#

AC_DEFUN(AM_SANITY_CHECK,
[AC_MSG_CHECKING([whether build environment is sane])
# Just in case
sleep 1
echo timestamp > conftestfile
# Do `set' in a subshell so we don't clobber the current shell's
# arguments.  Must try -L first in case configure is actually a
# symlink; some systems play weird games with the mod time of symlinks
# (eg FreeBSD returns the mod time of the symlink's containing
# directory).
if (
   set X `ls -Lt $srcdir/configure conftestfile 2> /dev/null`
   if test "[$]*" = "X"; then
      # -L didn't work.
      set X `ls -t $srcdir/configure conftestfile`
   fi
   if test "[$]*" != "X $srcdir/configure conftestfile" \
      && test "[$]*" != "X conftestfile $srcdir/configure"; then

      # If neither matched, then we have a broken ls.  This can happen
      # if, for instance, CONFIG_SHELL is bash and it inherits a
      # broken ls alias from the environment.  This has actually
      # happened.  Such a system could not be considered "sane".
      AC_MSG_ERROR([ls -t appears to fail.  Make sure there is not a broken
alias in your environment])
   fi

   test "[$]2" = conftestfile
   )
then
   # Ok.
   :
else
   AC_MSG_ERROR([newly created file is older than distributed files!
Check your system clock])
fi
rm -f conftest*
AC_MSG_RESULT(yes)])

dnl AM_MISSING_PROG(NAME, PROGRAM, DIRECTORY)
dnl The program must properly implement --version.
AC_DEFUN(AM_MISSING_PROG,
[AC_MSG_CHECKING(for working $2)
# Run test in a subshell; some versions of sh will print an error if
# an executable is not found, even if stderr is redirected.
# Redirect stdin to placate older versions of autoconf.  Sigh.
if ($2 --version) < /dev/null > /dev/null 2>&1; then
   $1=$2
   AC_MSG_RESULT(found)
else
   $1="$3/missing $2"
   AC_MSG_RESULT(missing)
fi
AC_SUBST($1)])

# Like AC_CONFIG_HEADER, but automatically create stamp file.

AC_DEFUN(AM_CONFIG_HEADER,
[AC_PREREQ([2.12])
AC_CONFIG_HEADER([$1])
dnl When config.status generates a header, we must update the stamp-h file.
dnl This file resides in the same directory as the config header
dnl that is generated.  We must strip everything past the first ":",
dnl and everything past the last "/".
AC_OUTPUT_COMMANDS(changequote(<<,>>)dnl
ifelse(patsubst(<<$1>>, <<[^ ]>>, <<>>), <<>>,
<<test -z "<<$>>CONFIG_HEADERS" || echo timestamp > patsubst(<<$1>>, <<^\([^:]*/\)?.*>>, <<\1>>)stamp-h<<>>dnl>>,
<<am_indx=1
for am_file in <<$1>>; do
  case " <<$>>CONFIG_HEADERS " in
  *" <<$>>am_file "*<<)>>
    echo timestamp > `echo <<$>>am_file | sed -e 's%:.*%%' -e 's%[^/]*$%%'`stamp-h$am_indx
    ;;
  esac
  am_indx=`expr "<<$>>am_indx" + 1`
done<<>>dnl>>)
changequote([,]))])


# serial 1

AC_DEFUN(AM_PATH_LISPDIR,
 [# If set to t, that means we are running in a shell under Emacs.
  # If you have an Emacs named "t", then use the full path.
  test "$EMACS" = t && EMACS=
  AC_PATH_PROGS(EMACS, emacs xemacs, no)
  if test $EMACS != "no"; then
    AC_MSG_CHECKING([where .elc files should go])
    dnl Set default value
    lispdir="\$(datadir)/emacs/site-lisp"
    emacs_flavor=`echo "$EMACS" | sed -e 's,^.*/,,'`
    if test "x$prefix" = "xNONE"; then
      if test -d $ac_default_prefix/share/$emacs_flavor/site-lisp; then
	lispdir="\$(prefix)/share/$emacs_flavor/site-lisp"
      else
	if test -d $ac_default_prefix/lib/$emacs_flavor/site-lisp; then
	  lispdir="\$(prefix)/lib/$emacs_flavor/site-lisp"
	fi
      fi
    else
      if test -d $prefix/share/$emacs_flavor/site-lisp; then
	lispdir="\$(prefix)/share/$emacs_flavor/site-lisp"
      else
	if test -d $prefix/lib/$emacs_flavor/site-lisp; then
	  lispdir="\$(prefix)/lib/$emacs_flavor/site-lisp"
	fi
      fi
    fi
    AC_MSG_RESULT($lispdir)
  fi
  AC_SUBST(lispdir)])

