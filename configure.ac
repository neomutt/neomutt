dnl Process this file with autoconf to produce a configure script.

dnl !!! WHEN ADDING NEW CONFIGURE TESTS, PLEASE ADD CODE TO MAIN.C !!!
dnl !!! TO DUMP THEIR RESULTS WHEN MUTT -V IS CALLED            !!!

AC_PREREQ([2.54])
AC_INIT([mutt.h])
AM_CONFIG_HEADER([config.h])

mutt_cv_version=`cat $srcdir/VERSION`
AM_INIT_AUTOMAKE(mutt, $mutt_cv_version)
AC_SUBST([CONFIG_STATUS_DEPENDENCIES], ['$(top_srcdir)/VERSION'])

AC_GNU_SOURCE

ALL_LINGUAS="de eu ru it es uk fr pl nl cs id sk ko el zh_TW zh_CN pt_BR eo gl sv da lt tr ja hu et ca bg ga"

AC_CANONICAL_HOST

AC_MSG_CHECKING([for prefix])
if test x$prefix = xNONE; then
  mutt_cv_prefix=$ac_default_prefix
else
  mutt_cv_prefix=$prefix
fi
AC_MSG_RESULT($mutt_cv_prefix)

AC_PROG_CC
AC_ISC_POSIX
AM_C_PROTOTYPES
if test "x$U" != "x"; then
  AC_MSG_ERROR(Compiler not ANSI compliant)
fi
AC_PROG_CPP
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_CHECK_TOOL(AR, ar, ar)

AC_C_INLINE
AC_C_CONST
AC_C_BIGENDIAN

AC_SYS_LARGEFILE
AC_FUNC_FSEEKO
AC_CHECK_SIZEOF(off_t)

AC_PATH_PROG(DBX, dbx, no)
AC_PATH_PROG(GDB, gdb, no)
AC_PATH_PROG(SDB, sdb, no)

if test $GDB != no ; then
        DEBUGGER=$GDB
elif test $DBX != no ; then
        DEBUGGER=$DBX
elif test $SDB != no ; then
        DEBUGGER=$SDB
else
        DEBUGGER=no
fi

AC_SUBST([DEBUGGER])

AH_TEMPLATE([sig_atomic_t],
            [/* Define to `int' if <signal.h> doesn't define.])
AH_TEMPLATE([HAVE_START_COLOR],
            [Define if you have start_color, as a function or macro.])
AH_TEMPLATE([HAVE_TYPEAHEAD],
            [Define if you have typeahead, as a function or macro.])
AH_TEMPLATE([HAVE_BKGDSET],
            [Define if you have bkgdset, as a function or macro.])
AH_TEMPLATE([HAVE_CURS_SET],
            [Define if you have curs_set, as a function or macro.])
AH_TEMPLATE([HAVE_META],
            [Define if you have meta, as a function or macro.])
AH_TEMPLATE([HAVE_USE_DEFAULT_COLORS],
            [Define if you have use_default_colors, as a function or macro.])
AH_TEMPLATE([HAVE_RESIZETERM],
            [Define if you have resizeterm, as a function or macro.])
AH_TEMPLATE([SIG_ATOMIC_VOLATILE_T],
            [Some systems declare sig_atomic_t as volatile, some others -- no.
             This define will have value `sig_atomic_t' or
             `volatile sig_atomic_t' accordingly.])
AH_TEMPLATE([ICONV_NONTRANS],
            [Define as 1 if iconv() only converts exactly and we should treat
             all return values other than (size_t)(-1) as equivalent.])

AH_BOTTOM([/* fseeko portability defines */
#ifdef HAVE_FSEEKO
# define LOFF_T off_t
# if HAVE_C99_INTTYPES && HAVE_INTTYPES_H
#  if SIZEOF_OFF_T == 8
#   define OFF_T_FMT "%" PRId64
#  else
#   define OFF_T_FMT "%" PRId32
#  endif
# else
#  if (SIZEOF_OFF_T == 8) && (SIZEOF_LONG == 4)
#   define OFF_T_FMT "%lld"
#  else
#   define OFF_T_FMT "%ld"
#  endif
# endif
#else
# define LOFF_T long
# define fseeko fseek
# define ftello ftell
# define OFF_T_FMT "%ld"
#endif
])
MUTT_C99_INTTYPES

ac_aux_path_sendmail=/usr/sbin:/usr/lib
AC_PATH_PROG(SENDMAIL, sendmail, /usr/sbin/sendmail, $PATH:$ac_aux_path_sendmail)
AC_DEFINE_UNQUOTED(SENDMAIL,"$ac_cv_path_SENDMAIL", [Where to find sendmail on your system.])

OPS='$(srcdir)/OPS'

AC_MSG_CHECKING([whether to build with GPGME support])
AC_ARG_ENABLE(gpgme, AC_HELP_STRING([--enable-gpgme], [Enable GPGME support]),
[       if test x$enableval = xyes; then
		enable_gpgme=yes
	fi
])

if test x"$enable_gpgme" = xyes; then
   AC_MSG_RESULT(yes)
   AM_PATH_GPGME(1.0.0, AC_DEFINE(CRYPT_BACKEND_GPGME, 1,
		 [Defined, if GPGME support is enabled]),
		 [gpgme_found=no])
   if test x"$gpgme_found" = xno; then
      AC_MSG_ERROR([*** GPGME not found ***])
   else
      AM_PATH_GPGME(1.1.1, AC_DEFINE(HAVE_GPGME_PKA_TRUST, 1,
				  [Define if GPGME supports PKA]))
      #needed to get GPGME_LIBS and al correctly
      AM_PATH_GPGME(1.0.0, AC_DEFINE(CRYPT_BACKEND_GPGME, 1,
                    [Define if you use GPGME to support OpenPGP]))
      MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS crypt-gpgme.o crypt-mod-pgp-gpgme.o crypt-mod-smime-gpgme.o"
   fi
else
   AC_MSG_RESULT([no])
fi

AC_ARG_ENABLE(pgp, AC_HELP_STRING([--disable-pgp], [Disable PGP support]),
[       if test x$enableval = xno ; then
                have_pgp=no
        fi
])

if test x$have_pgp != xno ; then
        AC_DEFINE(CRYPT_BACKEND_CLASSIC_PGP,1, [Define if you want classic PGP support.])
        PGPAUX_TARGET="pgpring\$(EXEEXT) pgpewrap\$(EXEEXT)"
        MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS pgp.o pgpinvoke.o pgpkey.o pgplib.o gnupgparse.o pgpmicalg.o pgppacket.o crypt-mod-pgp-classic.o"
fi

AC_ARG_ENABLE(smime, AC_HELP_STRING([--disable-smime], [Disable SMIME support]),
[	if test x$enableval = xno ; then
		have_smime=no
	fi
])
 
if test x$have_smime != xno ; then
	AC_DEFINE(CRYPT_BACKEND_CLASSIC_SMIME, 1, [Define if you want clasic S/MIME support.])
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS smime.o crypt-mod-smime-classic.o"
	SMIMEAUX_TARGET="smime_keys"
fi

AC_ARG_WITH(mixmaster, AC_HELP_STRING([--with-mixmaster@<:@=PATH@:>@], [Include Mixmaster support]),
  [if test "$withval" != no
   then
     if test -x "$withval"
     then
       MIXMASTER="$withval"
     else
       MIXMASTER="mixmaster"
     fi
     OPS="$OPS \$(srcdir)/OPS.MIX"
     MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS remailer.o"
     AC_DEFINE_UNQUOTED(MIXMASTER,"$MIXMASTER", [Where to find mixmaster on your system.])
   fi])

# We now require all OPS
OPS="$OPS \$(srcdir)/OPS.PGP \$(srcdir)/OPS.SMIME \$(srcdir)/OPS.CRYPT "
AC_SUBST([OPS])

AC_SUBST(PGPAUX_TARGET)
AC_SUBST(SMIMEAUX_TARGET)

AC_PATH_PROG(ISPELL, ispell, no)
if test $ISPELL != no; then
        AC_DEFINE_UNQUOTED(ISPELL,"$ISPELL",[ Where to find ispell on your system. ])
fi

AC_ARG_WITH(slang, AC_HELP_STRING([--with-slang@<:@=DIR@:>@], [Use S-Lang instead of ncurses]),
        [AC_CACHE_CHECK([if this is a BSD system], mutt_cv_bsdish,
                [AC_TRY_RUN([#include <sys/param.h>
#include <stdlib.h>

main ()
{
#ifdef BSD
        exit (0);
#else
        exit (1);
#endif
}],
                        mutt_cv_bsdish=yes,
                        mutt_cv_bsdish=no,
                        mutt_cv_bsdish=no)])

        AC_MSG_CHECKING(for S-Lang)
        if test $withval = yes; then
                if test -d $srcdir/../slang; then
                        mutt_cv_slang=$srcdir/../slang/src
                        CPPFLAGS="$CPPFLAGS -I${mutt_cv_slang}"
                        LDFLAGS="$LDFLAGS -L${mutt_cv_slang}/objs"
                else
                        if test -d $mutt_cv_prefix/include/slang; then
                                CPPFLAGS="$CPPFLAGS -I$mutt_cv_prefix/include/slang"
                        elif test -d /usr/include/slang; then
                                CPPFLAGS="$CPPFLAGS -I/usr/include/slang"
                        fi
                        mutt_cv_slang=yes
                fi
        else
                dnl ---Check to see if $withval is a source directory
                if test -f $withval/src/slang.h; then
                        mutt_cv_slang=$withval/src
                        CPPFLAGS="$CPPFLAGS -I${mutt_cv_slang}"
                        LDFLAGS="$LDFLAGS -L${mutt_cv_slang}/objs"
                else
                        dnl ---Must be installed somewhere
                        mutt_cv_slang=$withval
                        if test -d $withval/include/slang; then
                                CPPFLAGS="$CPPFLAGS -I${withval}/include/slang"
                        elif test -d $withval/include; then
                                CPPFLAGS="$CPPFLAGS -I${withval}/include"
                        fi
                        LDFLAGS="$LDFLAGS -L${withval}/lib"
                fi
        fi
        AC_MSG_RESULT($mutt_cv_slang)
        if test $mutt_cv_bsdish = yes; then
                AC_CHECK_LIB(termlib, main)
        fi
        AC_DEFINE(USE_SLANG_CURSES,1,
                [ Define if you compile with SLang instead of curses/ncurses. ])
        AC_DEFINE(HAVE_COLOR,1,[ Define if your curses library supports color. ])
        MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS resize.o"

        dnl --- now that we've found it, check the link

        AC_CHECK_LIB(slang, SLtt_get_terminfo,
                [MUTTLIBS="$MUTTLIBS -lslang -lm"],
                [AC_MSG_ERROR(unable to compile.  check config.log)], -lm)

        ],

        [mutt_cv_curses=/usr
        AC_ARG_WITH(curses, AC_HELP_STRING([--with-curses=DIR], [Where ncurses is installed]),
                [if test $withval != yes; then
                        mutt_cv_curses=$withval
                fi
                if test x$mutt_cv_curses != x/usr; then
                        LDFLAGS="$LDFLAGS -L${mutt_cv_curses}/lib"
                        CPPFLAGS="$CPPFLAGS -I${mutt_cv_curses}/include"
                fi])

	AC_CHECK_FUNC(initscr,,[
	cf_ncurses="ncurses"
	for lib in ncurses ncursesw
	do
		AC_CHECK_LIB($lib, waddnwstr, [cf_ncurses="$lib"; break])
	done
        AC_CHECK_LIB($cf_ncurses, initscr,
                [MUTTLIBS="$MUTTLIBS -l$cf_ncurses"
		
                if test "$cf_ncurses" = ncursesw; then
			AC_CHECK_HEADERS(ncursesw/ncurses.h,[cf_cv_ncurses_header="ncursesw/ncurses.h"])
		else
                  AC_CHECK_HEADERS(ncurses/ncurses.h,[cf_cv_ncurses_header="ncurses/ncurses.h"],
                    [AC_CHECK_HEADERS(ncurses.h,[cf_cv_ncurses_header="ncurses.h"])])
		fi],

                [CF_CURSES_LIBS])
		])

        old_LIBS="$LIBS"
        LIBS="$LIBS $MUTTLIBS"
        CF_CHECK_FUNCDECLS([#include <${cf_cv_ncurses_header-curses.h}>],
                           [start_color typeahead bkgdset curs_set meta use_default_colors resizeterm])
        if test "$ac_cv_func_decl_start_color" = yes; then
                AC_DEFINE(HAVE_COLOR,1,[ Define if your curses library supports color. ])
        fi
        if test "$ac_cv_func_decl_resizeterm" = yes; then
                MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS resize.o"
        fi
        LIBS="$old_LIBS"
        ])

AC_HEADER_STDC

AC_CHECK_HEADERS(stdarg.h sys/ioctl.h ioctl.h sysexits.h)
AC_CHECK_HEADERS(sys/time.h sys/resource.h)
AC_CHECK_HEADERS(unix.h)

AC_CHECK_FUNCS(setrlimit getsid)

AC_TYPE_SIGNAL

AC_MSG_CHECKING(for sig_atomic_t in signal.h)
AC_EGREP_HEADER(sig_atomic_t,signal.h,
  [
    ac_cv_type_sig_atomic_t=yes;
    AC_EGREP_HEADER(volatile.*sig_atomic_t,
                    signal.h,
                    [
                        is_sig_atomic_t_volatile=yes;
                        AC_MSG_RESULT([yes, volatile])
                    ],
                    [
                        is_sig_atomic_t_volatile=no;
                        AC_MSG_RESULT([yes, non volatile])
                    ])
  ],
  [
    AC_MSG_RESULT(no)
    AC_CHECK_TYPE(sig_atomic_t, int)
    is_sig_atomic_t_volatile=no
  ])
if test $is_sig_atomic_t_volatile = 'yes'
then
    AC_DEFINE(SIG_ATOMIC_VOLATILE_T, sig_atomic_t)
else
    AC_DEFINE(SIG_ATOMIC_VOLATILE_T, [volatile sig_atomic_t])
fi

AC_DECL_SYS_SIGLIST

AC_TYPE_PID_T
AC_CHECK_TYPE(ssize_t, int)

AC_CHECK_FUNCS(fgetpos memmove setegid srand48 strerror)

AC_REPLACE_FUNCS([setenv strcasecmp strdup strsep strtok_r wcscasecmp])
AC_REPLACE_FUNCS([strcasestr mkdtemp])

AC_CHECK_FUNC(getopt)
if test $ac_cv_func_getopt = yes; then
        AC_CHECK_HEADERS(getopt.h)
fi

SNPRINTFOBJS=""
AC_CHECK_FUNC(snprintf, [mutt_cv_func_snprintf=yes], [mutt_cv_func_snprintf=no])
AC_CHECK_FUNC(vsnprintf, [mutt_cv_func_vsnprintf=yes], [mutt_cv_func_vsnprintf=no])
if test $mutt_cv_func_snprintf = yes; then
AC_CACHE_CHECK([whether your system's snprintf is C99 compliant],
        [mutt_cv_c99_snprintf],
        AC_TRY_RUN([
#include <stdio.h>
int main()
{
changequote(, )dnl
  char buf[8];
  int len = snprintf(buf, 4, "1234567");
  return (len != 7 || buf[3] != '\0');
changequote([, ])dnl
}
        ], mutt_cv_c99_snprintf=yes, mutt_cv_c99_snprintf=no, mutt_cv_c99_snprintf=no))
else
        mutt_cv_c99_snprintf=no
fi
if test $mutt_cv_func_vsnprintf = yes; then
AC_CACHE_CHECK([whether your system's vsnprintf is C99 compliant],
        [mutt_cv_c99_vsnprintf],
        AC_TRY_RUN([
#include <stdarg.h>
#include <stdio.h>
int foo(const char *fmt, ...)
{
changequote(, )dnl
  char buf[8];
  int len;
  va_list ap;
  va_start(ap, fmt);
  len = vsnprintf(buf, 4, fmt, ap);
  va_end(ap);
  return (len != 7 || buf[3] != '\0');
changequote([, ])dnl
}

int main()
{
  return foo("%s", "1234567");
}
        ], mutt_cv_c99_vsnprintf=yes, mutt_cv_c99_vsnprintf=no, mutt_cv_c99_vsnprintf=no))
else
        mutt_cv_c99_vsnprintf=no
fi
if test $mutt_cv_c99_snprintf = yes; then
        AC_DEFINE(HAVE_SNPRINTF, 1, [ Define to 1 if you have a C99 compliant snprintf function. ])
fi
if test $mutt_cv_c99_vsnprintf = yes; then
        AC_DEFINE(HAVE_VSNPRINTF, 1, [ Define to 1 if you have a C99 compliant vsnprintf function. ])
fi
if test $mutt_cv_c99_snprintf = no -o $mutt_cv_c99_vsnprintf = no; then
        AC_LIBOBJ(snprintf)
fi

XIPH_FUNC_VA_COPY

dnl SCO uses chsize() instead of ftruncate()
AC_CHECK_FUNCS(ftruncate, , [AC_CHECK_LIB(x, chsize)])

dnl SCO has strftime() in libintl
AC_CHECK_FUNCS(strftime, , [AC_CHECK_LIB(intl, strftime)])

dnl AIX may not have fchdir()
AC_CHECK_FUNCS(fchdir, , [mutt_cv_fchdir=no])

AC_ARG_WITH(regex, AC_HELP_STRING([--with-regex], [Use the GNU regex library]),
        [mutt_cv_regex=yes],
        [AC_CHECK_FUNCS(regcomp, mutt_cv_regex=no, mutt_cv_regex=yes)])

if test $mutt_cv_regex = no ; then
AC_CACHE_CHECK([whether your system's regexp library is completely broken],
        [mutt_cv_regex_broken],
        AC_TRY_RUN([
#include <unistd.h>
#include <regex.h>
main() { regex_t blah ; regmatch_t p; p.rm_eo = p.rm_eo; return regcomp(&blah, "foo.*bar", REG_NOSUB) || regexec (&blah, "foobar", 0, NULL, 0); }],
        mutt_cv_regex_broken=no, mutt_cv_regex_broken=yes, mutt_cv_regex_broken=yes))
        if test $mutt_cv_regex_broken = yes ; then
                echo "Using the included GNU regex instead." >&AC_FD_MSG
                mutt_cv_regex=yes
        fi
fi

if test $mutt_cv_regex = yes; then
        AC_DEFINE(USE_GNU_REGEX,1,[ Define if you want to use the included regex.c. ])
        AC_LIBOBJ(regex)
fi


AC_ARG_WITH(homespool,
  AC_HELP_STRING([--with-homespool@<:@=FILE@:>@], [File in user's directory where new mail is spooled]), with_homespool=${withval})
if test x$with_homespool != x; then
        if test $with_homespool = yes; then
                with_homespool=mailbox
        fi
        AC_DEFINE_UNQUOTED(MAILPATH,"$with_homespool",[ Where new mail is spooled. ])
        AC_DEFINE(HOMESPOOL,1,
          [Is mail spooled to the user's home directory?  If defined,
           MAILPATH should be set to the filename of the spool mailbox
           relative the the home directory.
           use: configure --with-homespool=FILE])
        AC_DEFINE(USE_DOTLOCK,1,[ Define to use dotlocking for mailboxes. ])
        mutt_cv_setgid=no
else
        AC_ARG_WITH(mailpath, AC_HELP_STRING([--with-mailpath=DIR], [Directory where spool mailboxes are located]),
                [mutt_cv_mailpath=$withval],
                [ AC_CACHE_CHECK(where new mail is stored, mutt_cv_mailpath,
                        [mutt_cv_mailpath=no
                        if test -d /var/mail; then
                                mutt_cv_mailpath=/var/mail
                        elif test -d /var/spool/mail; then
                                mutt_cv_mailpath=/var/spool/mail
                        elif test -d /usr/spool/mail; then
                                mutt_cv_mailpath=/usr/spool/mail
                        elif test -d /usr/mail; then
                                mutt_cv_mailpath=/usr/mail
                        fi])
                ])
        if test "$mutt_cv_mailpath" = no; then
                AC_MSG_ERROR("Could not determine where new mail is stored.")
        fi
        AC_DEFINE_UNQUOTED(MAILPATH,"$mutt_cv_mailpath",[ Where new mail is spooled. ])

        AC_CACHE_CHECK(if $mutt_cv_mailpath is world writable, mutt_cv_worldwrite, [AC_TRY_RUN([#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

int main (int argc, char **argv)
{
        struct stat s;

        stat ("$mutt_cv_mailpath", &s);
        if (s.st_mode & S_IWOTH) exit (0);
        exit (1);
}], mutt_cv_worldwrite=yes, mutt_cv_worldwrite=no, mutt_cv_worldwrite=no)])

        mutt_cv_setgid=no
        if test $mutt_cv_worldwrite = yes; then
                AC_DEFINE(USE_DOTLOCK,1,[ Define to use dotlocking for mailboxes. ])
        else

                AC_CACHE_CHECK(if $mutt_cv_mailpath is group writable, mutt_cv_groupwrite, [AC_TRY_RUN([#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

int main (int argc, char **argv)
{
        struct stat s;

        stat ("$mutt_cv_mailpath", &s);
        if (s.st_mode & S_IWGRP) exit (0);
        exit (1);
}], mutt_cv_groupwrite=yes, mutt_cv_groupwrite=no, mutt_cv_groupwrite=no)])

                if test $mutt_cv_groupwrite = yes; then
                        AC_DEFINE(USE_DOTLOCK,1,[ Define to use dotlocking for mailboxes. ])
                        AC_DEFINE(USE_SETGID,1,[ Define if mutt should run setgid "mail". ])
                        mutt_cv_setgid=yes
                fi
        fi
fi

AC_ARG_ENABLE(external_dotlock, AC_HELP_STRING([--enable-external-dotlock], [Force use of an external dotlock program]),
        [mutt_cv_external_dotlock="$enableval"])

if test "x$mutt_cv_setgid" = "xyes" || test "x$mutt_cv_fchdir" = "xno" \
        || test "x$mutt_cv_external_dotlock" = "xyes"
then
        AC_DEFINE(DL_STANDALONE,1,[ Define if you want to use an external dotlocking program. ])
        DOTLOCK_TARGET="mutt_dotlock\$(EXEEXT)"
else
        MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS dotlock.o"
fi

AC_SUBST(DOTLOCK_TARGET)

dnl autoconf <2.60 compatibility
if test -z "$datarootdir"; then
  datarootdir='${prefix}/share'
fi
AC_SUBST([datarootdir])

AC_MSG_CHECKING(where to put the documentation)
AC_ARG_WITH(docdir, AC_HELP_STRING([--with-docdir=PATH], [Specify where to put the documentation]),
        [mutt_cv_docdir=$withval],
        [mutt_cv_docdir='${datarootdir}/doc/mutt'])
AC_MSG_RESULT($mutt_cv_docdir)

docdir=$mutt_cv_docdir
AC_SUBST(docdir)

if test x$mutt_cv_setgid = xyes; then
        DOTLOCK_GROUP='mail'
        DOTLOCK_PERMISSION=2755
else
        DOTLOCK_GROUP=''
        DOTLOCK_PERMISSION=755
fi
AC_SUBST(DOTLOCK_GROUP)
AC_SUBST(DOTLOCK_PERMISSION)

AC_ARG_WITH(domain, AC_HELP_STRING([--with-domain=DOMAIN], [Specify your DNS domain name]),
        [if test $withval != yes; then
            if test $withval != no; then
                AC_DEFINE_UNQUOTED(DOMAIN,"$withval",[ Define your domain name. ])
            fi
        fi])

need_socket="no"

dnl -- socket dependencies --

AC_ARG_ENABLE(pop,  AC_HELP_STRING([--enable-pop], [Enable POP3 support]),
[       if test x$enableval = xyes ; then
                AC_DEFINE(USE_POP,1,[ Define if you want support for the POP3 protocol. ])
                MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS pop.o pop_lib.o pop_auth.o"
                need_pop="yes"
                need_socket="yes"
                need_md5="yes"
        fi
])

AC_ARG_ENABLE(imap, AC_HELP_STRING([--enable-imap], [Enable IMAP support]),
[       if test x$enableval = xyes ; then
                AC_DEFINE(USE_IMAP,1,[ Define if you want support for the IMAP protocol. ])
                LIBIMAP="-Limap -limap"
                LIBIMAPDEPS="\$(top_srcdir)/imap/imap.h imap/libimap.a"
                need_imap="yes"
                need_socket="yes"
                need_md5="yes"
        fi
])
AM_CONDITIONAL(BUILD_IMAP, test x$need_imap = xyes)

AC_ARG_ENABLE(smtp, AC_HELP_STRING([--enable-smtp], [include internal SMTP relay support]),
	[if test $enableval = yes; then
		AC_DEFINE(USE_SMTP, 1, [Include internal SMTP relay support])
            	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS smtp.o"
		need_socket="yes"
	fi])

if test x"$need_imap" = xyes -o x"$need_pop" = xyes ; then
  MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS bcache.o"
fi

dnl -- end socket dependencies --

if test "$need_socket" = "yes"
then
        AC_CHECK_HEADERS([sys/select.h])
        AC_MSG_CHECKING([for socklen_t])
        AC_EGREP_HEADER(socklen_t, sys/socket.h, AC_MSG_RESULT([yes]),
                AC_MSG_RESULT([no])
                AC_DEFINE(socklen_t,int,
                        [ Define to 'int' if <sys/socket.h> doesn't have it. ]))
        AC_CHECK_FUNC(gethostent, , AC_CHECK_LIB(nsl, gethostent))
        AC_CHECK_FUNC(setsockopt, , AC_CHECK_LIB(socket, setsockopt))
        AC_CHECK_FUNCS(getaddrinfo)
        AC_DEFINE(USE_SOCKET,1,
                [ Include code for socket support. Set automatically if you enable POP3 or IMAP ])
        MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS account.o mutt_socket.o mutt_tunnel.o"
fi

dnl -- imap dependencies --

AC_ARG_WITH(gss, AC_HELP_STRING([--with-gss@<:@=PFX@:>@], [Compile in GSSAPI authentication for IMAP]), 
    gss_prefix="$withval", gss_prefix="no")
if test "$gss_prefix" != "no"
then
  if test "$need_imap" = "yes"
  then
    MUTT_AM_PATH_GSSAPI(gss_prefix)
    AC_MSG_CHECKING(GSSAPI implementation)
    AC_MSG_RESULT($GSSAPI_IMPL)
    if test "$GSSAPI_IMPL" = "none"
    then
      AC_CACHE_SAVE
      AC_MSG_RESULT([GSSAPI libraries not found])
    fi
    if test "$GSSAPI_IMPL" = "Heimdal"
    then
      AC_DEFINE(HAVE_HEIMDAL,1,[ Define if your GSSAPI implementation is Heimdal ])
    fi
    CPPFLAGS="$CPPFLAGS $GSSAPI_CFLAGS"
    MUTTLIBS="$MUTTLIBS $GSSAPI_LIBS"
    AC_DEFINE(USE_GSS,1,[ Define if you have GSSAPI libraries available ])
    need_gss="yes"
  else
    AC_MSG_WARN([GSS was requested but IMAP is not enabled])
  fi
fi
AM_CONDITIONAL(USE_GSS, test x$need_gss = xyes)

dnl -- end imap dependencies --

AC_ARG_WITH(ssl, AC_HELP_STRING([--with-ssl@<:@=PFX@:>@], [Enable TLS support using OpenSSL]),
[       if test "$with_ssl" != "no"
        then
          if test "$need_socket" != "yes"; then
           AC_MSG_WARN([SSL support is only useful with POP, IMAP or SMTP support])
	  else
            if test "$with_ssl" != "yes"
            then
             LDFLAGS="$LDFLAGS -L$withval/lib"
             CPPFLAGS="$CPPFLAGS -I$withval/include"
            fi
            saved_LIBS="$LIBS"

            crypto_libs=""
            AC_CHECK_LIB(z, deflate, [crypto_libs=-lz])
            AC_CHECK_LIB(crypto, X509_new,
              [crypto_libs="-lcrypto $crypto_libs"],, [$crypto_libs])
            AC_CHECK_LIB(ssl, SSL_new,,
              AC_MSG_ERROR([Unable to find SSL library]), [$crypto_libs])

            LIBS="$LIBS $crypto_libs"
            AC_CHECK_FUNCS(RAND_status RAND_egd)

            AC_DEFINE(USE_SSL,1,[ Define if you want support for SSL. ])
            AC_DEFINE(USE_SSL_OPENSSL,1,[ Define if you want support for SSL via OpenSSL. ])
            LIBS="$saved_LIBS"
            MUTTLIBS="$MUTTLIBS -lssl $crypto_libs"
            MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS mutt_ssl.o"
            need_ssl=yes
          fi
        fi
])

AC_ARG_WITH([gnutls], AC_HELP_STRING([--with-gnutls@<:@=PFX@:>@], [enable TLS support using gnutls]),
    [gnutls_prefix="$withval"], [gnutls_prefix="no"])
if test "$gnutls_prefix" != "no" && test x"$need_ssl" != xyes
then
  if test "$need_socket" != "yes"
  then
    AC_MSG_WARN([SSL support is only useful with POP, IMAP or SMTP support])
  else
    if test "$gnutls_prefix" != "yes"
    then
      LDFLAGS="$LDFLAGS -L$gnutls_prefix/lib"
      CPPFLAGS="$CPPFLAGS -I$gnutls_prefix/include"
    fi
    saved_LIBS="$LIBS"

    AC_CHECK_LIB(gnutls, gnutls_check_version,
      [dnl GNUTLS found
      AC_CHECK_DECLS([GNUTLS_VERIFY_DISABLE_TIME_CHECKS], [], [],
                    [[#include <gnutls/x509.h>]])

      LIBS="$saved_LIBS"
      MUTTLIBS="$MUTTLIBS -lgnutls"

      AC_DEFINE(USE_SSL, 1, [ Define if you want support for SSL. ])
      AC_DEFINE(USE_SSL_GNUTLS, 1, [ Define if you want support for SSL via GNUTLS. ])

      MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS mutt_ssl_gnutls.o"
      need_ssl=yes],
      [AC_MSG_ERROR([could not find libgnutls])])
  fi
fi

AM_CONDITIONAL(USE_SSL, test x$need_ssl = xyes)

AC_ARG_WITH(sasl, AC_HELP_STRING([--with-sasl@<:@=PFX@:>@], [Use Cyrus SASL 2 network security library]),
        [	
        if test "$with_sasl" != "no"
        then
          if test "$need_socket" != "yes"
          then
            AC_MSG_ERROR([SASL support is only useful with POP or IMAP support])
          fi

          if test "$with_sasl" != "yes"
          then
            CPPFLAGS="$CPPFLAGS -I$with_sasl/include"
            LDFLAGS="$LDFLAGS -L$with_sasl/lib"
          fi

          saved_LIBS="$LIBS"

          AC_CHECK_LIB(sasl2, sasl_client_init,,
            AC_MSG_ERROR([could not find libsasl2]),)

          MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS mutt_sasl.o"
          MUTTLIBS="$MUTTLIBS -lsasl2"
          LIBS="$saved_LIBS"
          AC_DEFINE(USE_SASL,1,
                  [ Define if want to use the Cyrus SASL library for POP/IMAP authentication. ])
          need_sasl=yes
        fi
        ])
AM_CONDITIONAL(USE_SASL, test x$need_sasl = xyes)

dnl -- end socket --

AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug], [Enable debugging support]),
        [ if test x$enableval = xyes ; then
                AC_DEFINE(DEBUG,1,[ Define to enable debugging info. ])
          fi
         ])

AC_ARG_ENABLE(flock, AC_HELP_STRING([--enable-flock], [Use flock() to lock files]),
        [if test $enableval = yes; then
                AC_DEFINE(USE_FLOCK,1, [ Define to use flock() to lock mailboxes. ])
        fi])

mutt_cv_fcntl=yes
AC_ARG_ENABLE(fcntl, AC_HELP_STRING([--disable-fcntl], [Do NOT use fcntl() to lock files]),
        [if test $enableval = no; then mutt_cv_fcntl=no; fi])

if test $mutt_cv_fcntl = yes; then
        AC_DEFINE(USE_FCNTL,1, [ Define to use fcntl() to lock folders. ])
fi

AC_MSG_CHECKING(whether struct dirent defines d_ino)
ac_cv_dirent_d_ino=no
AC_TRY_LINK([#include <dirent.h>],[struct dirent dp; (void)dp.d_ino],[ac_cv_dirent_d_ino=yes])
if test x$ac_cv_dirent_d_ino = xyes ; then
  AC_DEFINE(HAVE_DIRENT_D_INO,1,
            [Define to 1 if your system has the dirent::d_ino member])
fi
AC_MSG_RESULT($ac_cv_dirent_d_ino)

mutt_cv_warnings=yes
AC_ARG_ENABLE(warnings, AC_HELP_STRING([--disable-warnings], [Turn off compiler warnings (not recommended)]),
[if test $enableval = no; then
        mutt_cv_warnings=no
fi])

if test x$GCC = xyes && test $mutt_cv_warnings = yes; then
  CFLAGS="-Wall -pedantic -Wno-long-long $CFLAGS"
fi

AC_ARG_ENABLE(nfs-fix, AC_HELP_STRING([--enable-nfs-fix], [Work around an NFS with broken attributes caching]),
        [if test x$enableval = xyes; then
                AC_DEFINE(NFS_ATTRIBUTE_HACK,1,
                  [Define if you have problems with mutt not detecting
                   new/old mailboxes over NFS.  Some NFS implementations
                   incorrectly cache the attributes of small files.])
        fi])

AC_ARG_ENABLE(mailtool, AC_HELP_STRING([--enable-mailtool], [Enable Sun mailtool attachments support]),
        [if test x$enableval = xyes; then
                AC_DEFINE(SUN_ATTACHMENT,1,[ Define to enable Sun mailtool attachments support. ])
        fi])

AC_ARG_ENABLE(locales-fix, AC_HELP_STRING([--enable-locales-fix], [The result of isprint() is unreliable]),
        [if test x$enableval = xyes; then
                AC_DEFINE(LOCALES_HACK,1,[ Define if the result of isprint() is unreliable. ])
        fi])

AC_ARG_WITH(exec-shell, AC_HELP_STRING([--with-exec-shell=SHELL], [Specify alternate shell (ONLY if /bin/sh is broken)]),
        [if test $withval != yes; then
                AC_DEFINE_UNQUOTED(EXECSHELL, "$withval",
                 [program to use for shell commands])
         else
                AC_DEFINE_UNQUOTED(EXECSHELL, "/bin/sh")
        fi],
        [AC_DEFINE_UNQUOTED(EXECSHELL, "/bin/sh")])

AC_ARG_ENABLE(exact-address, AC_HELP_STRING([--enable-exact-address], [Enable regeneration of email addresses]),
        [if test $enableval = yes; then
                AC_DEFINE(EXACT_ADDRESS,1,
                  [Enable exact regeneration of email addresses as parsed?
                   NOTE: this requires significant more memory when defined.])

        fi])

dnl -- start cache --
db_found=no
db_requested=auto
AC_ARG_ENABLE(hcache, AC_HELP_STRING([--enable-hcache],
  [Enable header caching]))
AC_ARG_WITH(tokyocabinet, AC_HELP_STRING([--without-tokyocabinet],
  [Don't use tokyocabinet even if it is available]))
AC_ARG_WITH(qdbm, AC_HELP_STRING([--without-qdbm],
  [Don't use qdbm even if it is available]))
AC_ARG_WITH(gdbm, AC_HELP_STRING([--without-gdbm],
  [Don't use gdbm even if it is available]))
AC_ARG_WITH(bdb, AC_HELP_STRING([--with-bdb@<:@=DIR@:>@],
  [Use BerkeleyDB4 if gdbm is not available]))

if test x$enable_hcache = xyes
then
    AC_DEFINE(USE_HCACHE, 1, [Enable header caching])
    MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS hcache.o"

    OLDCPPFLAGS="$CPPFLAGS"
    OLDLDFLAGS="$LDFLAGS"
    OLDLIBS="$LIBS"

    need_md5="yes"

    if test -n "$with_tokyocabinet" && test "$with_tokyocabinet" != "no"
    then
      db_requested=tc
    fi
    if test -n "$with_qdbm" && test "$with_qdbm" != "no"
    then
      if test "$db_requested" != "auto"
      then
        AC_MSG_ERROR([more than one header cache engine requested.])
      else
        db_requested=qdbm
      fi
    fi
    if test -n "$with_gdbm" && test "$with_gdbm" != "no"
    then
      if test "$db_requested" != "auto"
      then
        AC_MSG_ERROR([more than one header cache engine requested.])
      else
        db_requested=gdbm
      fi
    fi
    if test -n "$with_bdb" && test "$with_bdb" != "no"
    then
      if test "$db_requested" != "auto"
      then
        AC_MSG_ERROR([more than one header cache engine requested.])
      else
        db_requested=bdb
      fi
    fi
    
    dnl -- Tokyo Cabinet --
    if test "$with_tokyocabinet" != "no" \
	    && test "$db_requested" = auto -o "$db_requested" = tc
    then
      if test -n "$with_tokyocabinet" && test "$with_tokyocabinet" != "yes"
      then
        CPPFLAGS="$CPPFLAGS -I$with_tokyocabinet/include"
        LDFLAGS="$LDFLAGS -L$with_tokyocabinet/lib"
      fi

      AC_CHECK_HEADER(tcbdb.h,
      AC_CHECK_LIB(tokyocabinet, tcbdbopen,
        [MUTTLIBS="$MUTTLIBS -ltokyocabinet"
         AC_DEFINE(HAVE_TC, 1, [Tokyo Cabinet Support])
         db_found=tc],
        [CPPFLAGS="$OLDCPPFLAGS"
         LDFLAGS="$OLDLDFLAGS"]))
      if test "$db_requested" != auto && test "$db_found" != "$db_requested"
      then
        AC_MSG_ERROR([Tokyo Cabinet could not be used. Check config.log for details.])
      fi
    fi

    dnl -- QDBM --
    if test "$with_qdbm" != "no" && test $db_found = no \
	    && test "$db_requested" = auto -o "$db_requested" = qdbm
    then
      if test -n "$with_qdbm" && test "$with_qdbm" != "yes"
      then
        if test -d $with_qdbm/include/qdbm; then
          CPPFLAGS="$CPPFLAGS -I$with_qdbm/include/qdbm"
	else
          CPPFLAGS="$CPPFLAGS -I$with_qdbm/include"
	fi
        LDFLAGS="$LDFLAGS -L$with_qdbm/lib"
      else
         if test -d /usr/include/qdbm; then
           CPPFLAGS="$CPPFLAGS -I/usr/include/qdbm"
	 fi
      fi

      saved_LIBS="$LIBS"
      AC_CHECK_HEADERS(villa.h)
      AC_CHECK_LIB(qdbm, vlopen,
        [MUTTLIBS="$MUTTLIBS -lqdbm"
         AC_DEFINE(HAVE_QDBM, 1, [QDBM Support])
         db_found=qdbm],
        [CPPFLAGS="$OLDCPPFLAGS"
         LDFLAGS="$OLDLDFLAGS"])
      LIBS="$saved_LIBS"
      if test "$db_requested" != auto && test "$db_found" != "$db_requested"
      then
        AC_MSG_ERROR([QDBM could not be used. Check config.log for details.])
      fi
    fi

    dnl -- GDBM --
    if test x$with_gdbm != xno && test $db_found = no \
	    && test "$db_requested" = auto -o "$db_requested" = gdbm
    then
        if test "$with_gdbm" != "yes"
        then
          CPPFLAGS="$CPPFLAGS -I$with_gdbm/include"
          LDFLAGS="$LDFLAGS -L$with_gdbm/lib"
        fi
        saved_LIBS="$LIBS"
        LIBS="$LIBS -lgdbm"
        AC_CACHE_CHECK(for gdbm_open, ac_cv_gdbmopen,[
            ac_cv_gdbmopen=no
            AC_TRY_LINK([#include <gdbm.h>],[gdbm_open(0,0,0,0,0);],[ac_cv_gdbmopen=yes])
        ])
        LIBS="$saved_LIBS"
        if test "$ac_cv_gdbmopen" = yes
        then
          AC_DEFINE(HAVE_GDBM, 1, [GDBM Support])
          MUTTLIBS="$MUTTLIBS -lgdbm"
          db_found=gdbm
        fi
        if test "$db_requested" != auto && test "$db_found" != "$db_requested"
        then
          AC_MSG_ERROR([GDBM could not be used. Check config.log for details.])
        fi
    fi

    dnl -- BDB --
    ac_bdb_prefix="$with_bdb"
    if test x$ac_bdb_prefix != xno && test $db_found = no
    then
        if test x$ac_bdb_prefix = xyes || test x$ac_bdb_prefix = x
        then
          ac_bdb_prefix="$mutt_cv_prefix /opt/csw/bdb4 /opt /usr/local /usr"
        fi
        for d in $ac_bdb_prefix; do
            bdbpfx="$bdbpfx $d"
            for v in BerkeleyDB.4.3 BerkeleyDB.4.2 BerkeleyDB.4.1; do
                bdbpfx="$bdbpfx $d/$v"
            done
        done
        BDB_VERSIONS="db-4 db4 db-4.6 db4.6 db46 db-4.5 db4.5 db45 db-4.4 db4.4 db44 db-4.3 db4.3 db43 db-4.2 db4.2 db42 db-4.1 db4.1 db41 db ''"
        AC_MSG_CHECKING([for BerkeleyDB > 4.0])
        for d in $bdbpfx; do
            BDB_INCLUDE_DIR=""
            BDB_LIB_DIR=""
            for v in / $BDB_VERSIONS; do
                if test -r "$d/include/$v/db.h"; then
                    BDB_INCLUDE_DIR="$d/include/$v"
                    for bdblibdir in "$d/lib/$v" "$d/lib"; do
                        test -d "$bdblibdir" || continue
                        BDB_LIB_DIR="$bdblibdir"
                        for l in `echo $BDB_VERSIONS`; do
                            CPPFLAGS="$OLDCPPFLAGS -I$BDB_INCLUDE_DIR"
                            LIBS="$OLDLIBS -L$BDB_LIB_DIR -l$l"
                            AC_TRY_LINK([
                                #include <stdlib.h>
                                #include <db.h>
                            ],[
                                DB *db = NULL;
                                db->open(db,NULL,NULL,NULL,0,0,0);
                            ],[
                                ac_cv_dbcreate=yes
                                BDB_LIB="$l"
                                break
                            ])
                        done
                        test x$ac_cv_dbcreate = xyes && break 2
                    done
                fi
            done
            test x$BDB_LIB != x && break
        done
        if test x$ac_cv_dbcreate = xyes
        then
            AC_MSG_RESULT(yes)
            CPPFLAGS="$OLDCPPFLAGS -I$BDB_INCLUDE_DIR"
            LIBS="$OLDLIBS -L$BDB_LIB_DIR -l$BDB_LIB"
            AC_DEFINE(HAVE_DB4, 1, [Berkeley DB4 Support])
            db_found=bdb
        else
            AC_MSG_RESULT(no)
        fi
    fi

    if test $db_found = no
    then
        AC_MSG_ERROR([You need Tokyo Cabinet, QDBM, GDBM or Berkeley DB4 for hcache])
    fi
fi
dnl -- end cache --

if test "$need_md5" = "yes"
then
  MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS md5.o"
  MUTT_MD5="mutt_md5"
fi
AC_SUBST(MUTT_MD5)

AC_SUBST(MUTTLIBS)
AC_SUBST(MUTT_LIB_OBJECTS)
AC_SUBST(LIBIMAP)
AC_SUBST(LIBIMAPDEPS)

dnl -- iconv/gettext --

AC_ARG_ENABLE(iconv, AC_HELP_STRING([--disable-iconv], [Disable iconv support]),
        [if test x$enableval = xno ; then
                am_cv_func_iconv=no
        fi
])

MUTT_AM_GNU_GETTEXT

if test "$am_cv_func_iconv" != "yes"
then
  AC_MSG_WARN([Configuring without iconv support. See INSTALL for details])
else

AC_CHECK_HEADERS(iconv.h,
        [AC_MSG_CHECKING(whether iconv.h defines iconv_t)
         AC_EGREP_HEADER([typedef.*iconv_t],iconv.h,
                [AC_MSG_RESULT(yes)
                 AC_DEFINE(HAVE_ICONV_T_DEF, 1,
                        [Define if <iconv.h> defines iconv_t.])],
                 AC_MSG_RESULT(no))])

dnl (1) Some implementations of iconv won't convert from UTF-8 to UTF-8.
dnl (2) In glibc-2.1.2 and earlier there is a bug that messes up ob and
dnl     obl when args 2 and 3 are 0 (fixed in glibc-2.1.3).
AC_CACHE_CHECK([whether this iconv is good enough], mutt_cv_iconv_good,
        mutt_save_LIBS="$LIBS"
        LIBS="$LIBS $LIBICONV"
        AC_TRY_RUN([
#include <iconv.h>
int main()
{
  iconv_t cd;
changequote(, )dnl
  char buf[4];
changequote([, ])dnl
  char *ob;
  size_t obl;
  ob = buf, obl = sizeof(buf);
  return ((cd = iconv_open("UTF-8", "UTF-8")) != (iconv_t)(-1) &&
          (iconv(cd, 0, 0, &ob, &obl) ||
           !(ob == buf && obl == sizeof(buf)) ||
           iconv_close(cd)));
}
                ],
                mutt_cv_iconv_good=yes,
                mutt_cv_iconv_good=no,
                mutt_cv_iconv_good=yes)
        LIBS="$mutt_save_LIBS")
if test "$mutt_cv_iconv_good" = no; then
  AC_MSG_ERROR(Try using libiconv instead)
fi

dnl This is to detect implementations such as the one in glibc-2.1,
dnl which always convert exactly but return the number of characters
dnl converted instead of the number converted inexactly.
AC_CACHE_CHECK([whether iconv is non-transcribing], mutt_cv_iconv_nontrans,
        mutt_save_LIBS="$LIBS"
        LIBS="$LIBS $LIBICONV"
        AC_TRY_RUN([
#include <iconv.h>
#include <string.h>
int main()
{
  iconv_t cd;
  const char *ib;
  char *ob;
  size_t ibl, obl;
  const char *s = "\304\211";
changequote(, )dnl
  char t[3];
changequote([, ])dnl
  ib = s, ibl = 2, ob = t, obl = 3;
  return ((cd = iconv_open("UTF-8", "UTF-8")) == (iconv_t)(-1) ||
          iconv(cd, &ib, &ibl, &ob, &obl));
}
                ],
                mutt_cv_iconv_nontrans=no,
                mutt_cv_iconv_nontrans=yes,
                mutt_cv_iconv_nontrans=no)
        LIBS="$mutt_save_LIBS")
if test "$mutt_cv_iconv_nontrans" = yes; then
  AC_DEFINE(ICONV_NONTRANS, 1)
else
  AC_DEFINE(ICONV_NONTRANS, 0)
fi

CPPFLAGS="$CPPFLAGS -I\$(top_srcdir)/intl"

if test "$BUILD_INCLUDED_LIBINTL" = "yes"; then
  AC_DEFINE(HAVE_BIND_TEXTDOMAIN_CODESET,1,
          [ Define if your gettext has bind_textdomain_codeset. ])
else
  mutt_save_LIBS="$LIBS"
  LIBS="$LIBS $INTLLIBS"
  AC_CHECK_FUNCS(bind_textdomain_codeset)
  LIBS="$mutt_save_LIBS"
fi

fi # libiconv

dnl -- IDN depends on iconv

AC_ARG_WITH(idn, AC_HELP_STRING([--with-idn=@<:@PFX@:>@], [Use GNU libidn for domain names]),
		 [
		  if test "$with_idn" != "no" ; then
		  	if test "$with_idn" != "yes" ; then
			   CPPFLAGS="$CPPFLAGS -I$with_idn/include"
			   LDFLAGS="$LDFLAGS -L$with_idn/lib"
			fi
		  fi
		 ]
)

if test "x$with_idn" != "xno"; then
  if test "$am_cv_func_iconv" != "yes"
  then
    if test "x$with_idn" != "x"
    then
      AC_MSG_ERROR([IDN requested but iconv is disabled or unavailable])
    fi
  else
    AC_SEARCH_LIBS([stringprep_check_version], [idn], [
      AC_DEFINE([HAVE_LIBIDN], 1, [Define to 1 if you have the `idn' library])
      MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS mutt_idna.o"
    ])
    AC_CHECK_FUNCS(idna_to_unicode_utf8_from_utf8 idna_to_unicode_8z8z)
    AC_CHECK_FUNCS(idna_to_ascii_from_utf8 idna_to_ascii_8z)
    AC_CHECK_FUNCS(idna_to_ascii_lz idna_to_ascii_from_locale)
  fi
fi

dnl -- locales --

AC_CHECK_HEADERS(wchar.h)

AC_CACHE_CHECK([for wchar_t], mutt_cv_wchar_t,
        AC_TRY_COMPILE([
#include <stddef.h>
#include <stdlib.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
                ],
                [ wchar_t wc; return 0; ],
                mutt_cv_wchar_t=yes,
                mutt_cv_wchar_t=no))

if test "$mutt_cv_wchar_t" = no; then
        AC_DEFINE(wchar_t,int,[ Define to 'int' if system headers don't define. ])
fi

AC_CACHE_CHECK([for wint_t], mutt_cv_wint_t,
        AC_TRY_COMPILE([
#include <stddef.h>
#include <stdlib.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
                ],
                [ wint_t wc; return 0; ],
                mutt_cv_wint_t=yes,
                mutt_cv_wint_t=no))

if test "$mutt_cv_wint_t" = no; then
        AC_DEFINE(wint_t,int,[ Define to 'int' if system headers don't define. ])
fi

AC_CHECK_HEADERS(wctype.h)
AC_CHECK_FUNCS(iswalnum iswalpha  iswcntrl iswdigit)
AC_CHECK_FUNCS(iswgraph iswlower iswprint iswpunct iswspace iswupper)
AC_CHECK_FUNCS(iswxdigit towupper towlower)

AC_CACHE_CHECK([for mbstate_t], mutt_cv_mbstate_t,
        AC_TRY_COMPILE([
#include <stddef.h>
#include <stdlib.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
                ],
                [ mbstate_t s; return 0; ],
                mutt_cv_mbstate_t=yes,
                mutt_cv_mbstate_t=no))

if test "$mutt_cv_mbstate_t" = no; then
        AC_DEFINE(mbstate_t,int,[ Define to 'int' if system headers don't define. ])
fi

wc_funcs=maybe
AC_ARG_WITH(wc-funcs, AC_HELP_STRING([--without-wc-funcs], [Do not use the system's wchar_t functions]),
        wc_funcs=$withval)

if test "$wc_funcs" != yes && test "$wc_funcs" != no; then
        AC_CACHE_CHECK([for wchar_t functions], mutt_cv_wc_funcs,
                mutt_cv_wc_funcs=no
                AC_TRY_LINK([
#define _XOPEN_SOURCE 1
#include <stddef.h>
#include <stdlib.h>
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif],
                        [mbrtowc(0, 0, 0, 0); wctomb(0, 0); wcwidth(0);
        iswprint(0); iswspace(0); towlower(0); towupper(0); iswalnum(0)],
                        mutt_cv_wc_funcs=yes))
        wc_funcs=$mutt_cv_wc_funcs
fi

if test $wc_funcs = yes; then
        AC_DEFINE(HAVE_WC_FUNCS,1,[ Define if you are using the system's wchar_t functions. ])
else
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS utf8.o wcwidth.o"
fi

AC_CACHE_CHECK([for nl_langinfo and CODESET], mutt_cv_langinfo_codeset,
  [AC_TRY_LINK([#include <langinfo.h>],
    [char* cs = nl_langinfo(CODESET);],
    mutt_cv_langinfo_codeset=yes,
    mutt_cv_langinfo_codeset=no)])
if test $mutt_cv_langinfo_codeset = yes; then
  AC_DEFINE(HAVE_LANGINFO_CODESET,1,[ Define if you have <langinfo.h> and nl_langinfo(CODESET). ])
fi

AC_CACHE_CHECK([for nl_langinfo and YESEXPR], mutt_cv_langinfo_yesexpr,
  [AC_TRY_LINK([#include <langinfo.h>],
    [char* cs = nl_langinfo(YESEXPR);],
    mutt_cv_langinfo_yesexpr=yes,
    mutt_cv_langinfo_yesexpr=no)])
if test $mutt_cv_langinfo_yesexpr = yes; then
  AC_DEFINE(HAVE_LANGINFO_YESEXPR,1,[ Define if you have <langinfo.h> and nl_langinfo(YESEXPR). ])
fi

dnl Documentation tools
have_openjade="no"
AC_PATH_PROG([OSPCAT], [ospcat], [none])
if test "$OSPCAT" != "none"
then
  AC_MSG_CHECKING([for openjade docbook stylesheets])
  dslosfile=`ospcat --public-id="-//Norman Walsh//DOCUMENT DocBook Print Stylesheet//EN"`
  DSLROOT=`echo $dslosfile | sed -n -e "s/.*SOIBASE='\(@<:@^'@:>@*\)\/catalog'.*/\1/p"`
  # ospcat may spit out an absolute path without an SOIBASE
  if test -z "$DSLROOT"
  then
    DSLROOT=`echo $dslosfile | sed -e 's|<OSFILE>\(.*\)/print/docbook.dsl|\1|'`
  fi
  if test -f $DSLROOT/print/docbook.dsl
  then
    AC_MSG_RESULT([in $DSLROOT])
    have_openjade="yes"
  else
    AC_MSG_RESULT([not found: PDF documentation will not be built.])
  fi
fi
AC_SUBST(DSLROOT)

AC_ARG_ENABLE(full_doc,
	      AC_HELP_STRING([--disable-full-doc], [Omit disabled variables]),
[       if test x$enableval = xno ; then
                full_doc=no
        fi
])
if test x$full_doc != xno ; then
  AC_DEFINE(MAKEDOC_FULL,1, [Define if you want complete documentation.])
fi

AC_OUTPUT(Makefile contrib/Makefile doc/Makefile imap/Makefile
        intl/Makefile m4/Makefile po/Makefile.in
        hcachever.sh muttbug.sh doc/instdoc.sh)
