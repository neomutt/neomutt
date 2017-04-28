dnl To create the configure script, run:
dnl     autoreconf -i

dnl !!! WHEN ADDING NEW CONFIGURE TESTS, PLEASE ADD CODE TO MAIN.C !!!
dnl !!! TO DUMP THEIR RESULTS WHEN MUTT -V IS CALLED               !!!

CFLAGS=$CFLAGS
LDFLAGS=$LDFLAGS

AC_INIT([NeoMutt], [20170428], [neomutt-devel@neomutt.org], [mutt], [https://www.neomutt.org])
AC_CONFIG_SRCDIR(mutt.h)
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

AC_DEFINE(MUTT_VERSION, "1.8.2", [Version of upstream Mutt])

AC_USE_SYSTEM_EXTENSIONS

ALL_LINGUAS="bg ca cs da de el en_GB eo es et eu fr ga gl hu id it ja ko lt nl pl pt_BR ru sk sv tr uk zh_CN zh_TW"

AC_CANONICAL_HOST

AC_MSG_CHECKING([for prefix])
if test x$prefix = xNONE; then
	mutt_cv_prefix=$ac_default_prefix
else
	mutt_cv_prefix=$prefix
fi
AC_MSG_RESULT($mutt_cv_prefix)

AC_PROG_CC
AC_PROG_CC_C99
if test "$ac_cv_prog_cc_c99" = "no"; then
	AC_ERROR([Compiler does not support C99. Aborting.])
fi

if test "$CC" = "gcc"; then
	CFLAGS="$CFLAGS -fno-delete-null-pointer-checks"
fi

AC_SEARCH_LIBS([strerror],[cposix])
if test "x$U" != "x"; then
	AC_MSG_ERROR(Compiler not ANSI compliant)
fi
AC_PROG_CPP
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PROG_MKDIR_P
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

if test $GDB != no; then
	DEBUGGER=$GDB
elif test $DBX != no; then
	DEBUGGER=$DBX
elif test $SDB != no; then
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
AC_TYPE_LONG_LONG_INT

ac_aux_path_sendmail=/usr/sbin:/usr/lib
AC_PATH_PROG(SENDMAIL, sendmail, /usr/sbin/sendmail, $PATH:$ac_aux_path_sendmail)
AC_DEFINE_UNQUOTED(SENDMAIL,"$ac_cv_path_SENDMAIL", [Where to find sendmail on your system.])

OPS='$(srcdir)/OPS'


dnl Define the option to enable everything before any feature / option is declared
dnl This is important because it allows us to set a default value for the options
dnl that can later be overridden by the user's command line options.
dnl Remember, the user is always the most powerful.

dnl When adding new features / options, **PLEASE** do not use the enable_* or
dnl with_* variables. They are also used by autoconf internally and will interfere
dnl with our logic. Also, remember to initialize the variable in both the true and
dnl the else case, otherwise it will appear blank in the configure summary.

AC_ARG_ENABLE(everything,
	AC_HELP_STRING([--enable-everything], [Enable all Options and Features]),
	[enable_everything=$enableval], [enable_everything=no])

AS_IF([test x$enable_everything = "xyes"], [
	use_gpgme="yes"
	use_pgp="yes"
	use_smime="yes"
	use_sidebar="yes"
	use_compressed="yes"
	use_notmuch="yes"
	use_pop="yes"
	use_imap="yes"
	use_nntp="yes"
	use_smtp="yes"
	use_lua="yes"
	hcache_tokyocabinet="yes"
	hcache_kyotocabinet="yes"
	hcache_bdb="yes"
	hcache_gdbm="yes"
	hcache_qdbm="yes"
	hcache_lmdb="yes"
], [
	use_gpgme="no"
	use_pgp="no"
	use_smime="no"
	use_sidebar="no"
	use_compressed="no"
	use_notmuch="no"
	use_pop="no"
	use_imap="no"
	use_nntp="no"
	use_smtp="no"
	use_lua="no"
])


dnl : A note about the organization of this file
dnl : =========================================================================
dnl : All features and options must be defined below the line marked as their
dnl : beginning. However only the configure option definitions should be there.
dnl : These definitions should do nothing more than set a variable indicating
dnl : whether the feature / option is enabled or disabled. All checks for
dnl : libraries and other changes should be made *AFTER* the
dnl : --enable-everything option is defined. There will be a line stating it is
dnl : safe to start running checks after it. Do *NOT* violate this rule, or you
dnl : risk incurring the wrath of Khan.

dnl : Also, please remember to use the following convention:
dnl : When a feature defaults to enabled, define both the cases in
dnl : AC_ARG_ENABLE. However, when a feature defaults to disabled, do *NOT*
dnl : write the `action-if-not-given` case. Explicitly disabling it has no
dnl : benefits, but will cause the --enable-everything options to not work

dnl == All Mutt Features and Options must be "defined" below this line ==

AC_ARG_ENABLE(gpgme,
	AS_HELP_STRING([--enable-gpgme], [Enable GPGME support]),
	[use_gpgme=$enableval])

AC_ARG_ENABLE(pgp,
	AS_HELP_STRING([--disable-pgp], [Disable PGP support]),
	[use_pgp=$enableval], [use_pgp=yes])

AC_ARG_ENABLE(smime,
	AS_HELP_STRING([--disable-smime], [Disable SMIME support]),
	[use_smime=$enableval], [use_smime=yes])

AC_ARG_ENABLE(sidebar,
	AC_HELP_STRING([--enable-sidebar], [Enable Sidebar support]),
	[use_sidebar=$enableval])

AC_ARG_ENABLE(lua,
	AC_HELP_STRING([--enable-lua], [Enable lua scripting support]),
	[use_lua=$enableval])

AC_ARG_ENABLE(compressed,
	AC_HELP_STRING([--enable-compressed], [Enable compressed folders support]),
	[use_compressed=$enableval])

AC_ARG_ENABLE(notmuch,
	AC_HELP_STRING([--enable-notmuch], [Enable NOTMUCH support]),
	[use_notmuch=$enableval])

AC_ARG_ENABLE(pop,
	AS_HELP_STRING([--enable-pop], [Enable POP3 support]),
	[use_pop=$enableval])

AC_ARG_ENABLE(imap,
	AS_HELP_STRING([--enable-imap], [Enable IMAP support]),
	[use_imap=$enableval])

AC_ARG_ENABLE(nntp,
	AC_HELP_STRING([--enable-nntp], [Enable NNTP support]),
	[use_nntp=$enableval])

AC_ARG_ENABLE(smtp,
	AS_HELP_STRING([--enable-smtp], [Include internal SMTP relay support]),
	[use_smtp=$enableval])

AC_ARG_WITH(mixmaster,
	AS_HELP_STRING([--with-mixmaster@<:@=PATH@:>@], [Include Mixmaster support]),
	[alongwith_mixmaster=$withval], [alongwith_mixmaster=no])

dnl == End of Features and Options Definitions ==


dnl == Declare all the checks and code for options / features below this line ==

dnl --enable-gpgme
AS_IF([test x$use_gpgme = "xyes"], [
	ifdef([AM_PATH_GPGME], [
		AM_PATH_GPGME(1.2.0, [
			AC_DEFINE(CRYPT_BACKEND_GPGME, 1, [GPGME support])
			MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS crypt_gpgme.o \
			crypt_mod_pgp_gpgme.o crypt_mod_smime_gpgme.o"
		], [gpgme_found=no])
	], [gpgme_found=no])
])
AS_IF([test x$use_gpgme = xyes && test x$gpgme_found = xno], [
	AC_MSG_ERROR([GPGME not found])
])

dnl --enable-pgp
AS_IF([test x$use_pgp != "xno"], [
	AC_DEFINE(CRYPT_BACKEND_CLASSIC_PGP, 1, [Define if you want classic PGP Support.])
	PGPAUX_TARGET="pgpring\$(EXEEXT) pgpewrap\$(EXEEXT)"
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS pgp.o pgpinvoke.o pgpkey.o pgplib.o gnupgparse.o pgpmicalg.o pgppacket.o crypt_mod_pgp_classic.o"
])

dnl --enable-smime
AS_IF([test x$use_smime != "xno"], [
	AC_DEFINE(CRYPT_BACKEND_CLASSIC_SMIME, 1, [Define if you want classic S/MIME support.])
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS smime.o crypt_mod_smime_classic.o"
	SMIMEAUX_TARGET="smime_keys"
])

dnl --enable-sidebar
AS_IF([test x$use_sidebar = "xyes"], [
	AC_DEFINE(USE_SIDEBAR, 1, [Define if you want support for the sidebar.])
	OPS="$OPS \$(srcdir)/OPS.SIDEBAR"
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS sidebar.o"
])

dnl --enable-compressed
AS_IF([test x$use_compressed = "xyes"], [
	AC_DEFINE(USE_COMPRESSED, 1, [Define to enable compressed folders support.])
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS compress.o"
])
AM_CONDITIONAL(BUILD_COMPRESS, test x$enable_compressed = xyes)

dnl --enable-lua
AS_IF([test x$use_lua = "xyes"], [
	AX_PROG_LUA([5.2],[],:,enable_lua=no)
	AX_LUA_HEADERS(:,enable_lua=no)
	AX_LUA_LIBS(:,enable_lua=no)
])

AS_IF([test x$enable_lua = "xyes"], [
	AC_DEFINE(USE_LUA, 1, [Define if you want support for LUA.])
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS mutt_lua.o"
	CPPFLAGS="$CPPFLAGS $LUA_INCLUDE"
	LIBS="$LIBS $LUA_LIB"
])
AM_CONDITIONAL(BUILD_LUA, test x$enable_lua = xyes)

dnl --enable-notmuch
AS_IF([test x$use_notmuch = "xyes"], [
	AC_CHECK_LIB(notmuch, notmuch_database_open,,
		AC_MSG_ERROR([Unable to find Notmuch library]))
	AC_DEFINE(USE_NOTMUCH,1,[ Define if you want support for the notmuch. ])
	NOTMUCH_LIBS="-lnotmuch"
	OPS="$OPS \$(srcdir)/OPS.NOTMUCH"
	need_notmuch="yes"

	AC_MSG_CHECKING([for notmuch api version 3])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
		[[#include <notmuch.h>]],
		[[notmuch_database_open("/path", NOTMUCH_DATABASE_MODE_READ_ONLY, (notmuch_database_t**)NULL);]]
	)], [
		notmuch_api_3=yes
		AC_DEFINE([NOTMUCH_API_3], 1, [Define to 1 if you have the notmuch api version 3.])
	], [
		notmuch_api_3=no
	])
	AC_MSG_RESULT([$notmuch_api_3])
])
AM_CONDITIONAL(BUILD_NOTMUCH, test x$need_notmuch = xyes)

dnl --with-mixmaster
AS_IF([test "$alongwith_mixmaster" != "no"], [
	AS_IF([test -x "$alongwith_mixmaster"], [MIXMASTER="$alongwith_mixmaster"], [MIXMASTER="mixmaster"])
	OPS="$OPS \$(srcdir)/OPS.MIX"
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS remailer.o"
	AC_DEFINE_UNQUOTED(MIXMASTER, "$MIXMASTER", [Where to find mixmaster on your system.])
])


# We now require all OPS
OPS="$OPS \$(srcdir)/OPS.PGP \$(srcdir)/OPS.SMIME \$(srcdir)/OPS.CRYPT "
AC_SUBST([OPS])

AC_SUBST(PGPAUX_TARGET)
AC_SUBST(SMIMEAUX_TARGET)

AC_PATH_PROG(ISPELL, ispell, no)
if test $ISPELL != no; then
	AC_DEFINE_UNQUOTED(ISPELL,"$ISPELL",[ Where to find ispell on your system. ])
fi

AC_ARG_WITH(slang, AS_HELP_STRING([--with-slang@<:@=DIR@:>@],[Use S-Lang instead of ncurses]),
	[AC_CACHE_CHECK([if this is a BSD system], mutt_cv_bsdish,
		[AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <sys/param.h>
#include <stdlib.h>
main ()
{
#ifdef BSD
  exit (0);
#else
  exit (1);
#endif
}]])],[mutt_cv_bsdish=yes],[mutt_cv_bsdish=no],[mutt_cv_bsdish=no])])

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
	AC_ARG_WITH(curses, AS_HELP_STRING([--with-curses=DIR],[Where ncurses is installed]),
		[if test x$withval != xyes; then
			mutt_cv_curses=$withval
		fi
		if test x$mutt_cv_curses != x/usr; then
			LDFLAGS="$LDFLAGS -L${mutt_cv_curses}/lib"
			CPPFLAGS="$CPPFLAGS -I${mutt_cv_curses}/include"
		fi])

	AC_CHECK_FUNC(initscr,,[
	cf_ncurses=""
	for lib in ncursesw ncurses
	do
		AC_CHECK_LIB($lib, waddnwstr, [cf_ncurses="$lib"; break])
	done
	AC_CHECK_LIB($cf_ncurses, initscr,
		[MUTTLIBS="$MUTTLIBS -l$cf_ncurses"

		AC_CHECK_LIB($cf_ncurses, tgetent, [], [
			AC_CHECK_LIB(tinfo, tgetent, [MUTTLIBS="$MUTTLIBS -ltinfo"])
		])

		if test "$cf_ncurses" = ncursesw; then
			AC_CHECK_HEADERS(ncursesw/ncurses.h,
				[cf_cv_ncurses_header="ncursesw/ncurses.h"],
				[AC_CHECK_HEADERS(ncurses.h,[cf_cv_ncurses_header="ncurses.h"])]
			)
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
	AC_CHECK_FUNCS([use_extended_names])
	LIBS="$old_LIBS"
	])

AC_HEADER_STDC

AC_CHECK_HEADERS(stdarg.h sys/ioctl.h ioctl.h sysexits.h)
AC_CHECK_HEADERS(sys/time.h sys/resource.h sys/syscall.h)
AC_CHECK_HEADERS(unix.h)

AC_CHECK_FUNCS(setrlimit getsid)
AC_CHECK_FUNCS(fgets_unlocked fgetc_unlocked)
AC_CHECK_FUNCS(strcasecmp strncasecmp setenv strdup strsep strtok_r mkdtemp)

AC_MSG_CHECKING(for sig_atomic_t in signal.h)
AC_EGREP_HEADER(sig_atomic_t,signal.h,
	[
		ac_cv_type_sig_atomic_t=yes;
		AC_EGREP_HEADER(volatile.*sig_atomic_t,
			signal.h,
			[
				is_sig_atomic_t_volatile=yes;
				AC_MSG_RESULT([yes, volatile])
			], [
				is_sig_atomic_t_volatile=no;
				AC_MSG_RESULT([yes, non volatile])
			])
	], [
		AC_MSG_RESULT(no)
		AC_CHECK_TYPE(sig_atomic_t, int)
		is_sig_atomic_t_volatile=no
	])
if test $is_sig_atomic_t_volatile = 'yes'; then
	AC_DEFINE(SIG_ATOMIC_VOLATILE_T, sig_atomic_t)
else
	AC_DEFINE(SIG_ATOMIC_VOLATILE_T, [volatile sig_atomic_t])
fi

AC_CHECK_DECLS([sys_siglist],[],[],[#include <signal.h>
/* NetBSD declares sys_siglist in unistd.h.  */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
])


AC_TYPE_PID_T
AC_CHECK_TYPE(ssize_t, int)

AC_CHECK_FUNCS(fgetpos memmove setegid srand48 strerror)

AC_REPLACE_FUNCS([wcscasecmp strcasestr])

AC_CHECK_FUNC(getopt)
if test $ac_cv_func_getopt = yes; then
	AC_CHECK_HEADERS(getopt.h)
fi

dnl SCO uses chsize() instead of ftruncate()
AC_CHECK_FUNCS(ftruncate, , [AC_CHECK_LIB(x, chsize)])

dnl SCO has strftime() in libintl
AC_CHECK_FUNCS(strftime, , [AC_CHECK_LIB(intl, strftime)])

dnl Set the atime of files
AC_CHECK_FUNCS(futimens)

dnl AIX may not have fchdir()
AC_CHECK_FUNCS(fchdir, , [mutt_cv_fchdir=no])

AC_ARG_WITH(homespool,
	AS_HELP_STRING([--with-homespool@<:@=FILE@:>@],[File in user's directory where new mail is spooled]), with_homespool=${withval})
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
	AC_ARG_WITH(mailpath, AS_HELP_STRING([--with-mailpath=DIR],[Directory where spool mailboxes are located]),
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

	AC_CACHE_CHECK(if $mutt_cv_mailpath is world writable, mutt_cv_worldwrite, [AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
int main (int argc, char **argv)
{
  struct stat s;
  if (stat ("$mutt_cv_mailpath", &s)) exit (1);
  if (s.st_mode & S_IWOTH) exit (0);
  exit (1);
}]])],[mutt_cv_worldwrite=yes],[mutt_cv_worldwrite=no],[mutt_cv_worldwrite=no])])

	mutt_cv_setgid=no
	if test $mutt_cv_worldwrite = yes; then
		AC_DEFINE(USE_DOTLOCK,1,[ Define to use dotlocking for mailboxes. ])
	else

		AC_CACHE_CHECK(if $mutt_cv_mailpath is group writable, mutt_cv_groupwrite, [AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
int main (int argc, char **argv)
{
  struct stat s;
  if (stat ("$mutt_cv_mailpath", &s)) exit (1);
  if (s.st_mode & S_IWGRP) exit (0);
  exit (1);
}]])],[mutt_cv_groupwrite=yes],[mutt_cv_groupwrite=no],[mutt_cv_groupwrite=no])])

		if test $mutt_cv_groupwrite = yes; then
			AC_DEFINE(USE_DOTLOCK,1,[ Define to use dotlocking for mailboxes. ])
			AC_DEFINE(USE_SETGID,1,[ Define if mutt should run setgid "mail". ])
			mutt_cv_setgid=yes
		fi
	fi
fi

AC_ARG_ENABLE(external_dotlock, AS_HELP_STRING([--enable-external-dotlock],[Force use of an external dotlock program]),
	[mutt_cv_external_dotlock="$enableval"])

if test "x$mutt_cv_setgid" = "xyes" || test "x$mutt_cv_fchdir" = "xno" \
	|| test "x$mutt_cv_external_dotlock" = "xyes"; then
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
AC_ARG_WITH(docdir, AS_HELP_STRING([--with-docdir=PATH],[Specify where to put the documentation]),
	[mutt_cv_docdir=$withval],
	[mutt_cv_docdir='${datarootdir}/doc/mutt'])
AC_MSG_RESULT($mutt_cv_docdir)
if test -z "$docdir" -o -n "$with_docdir"; then
	docdir=$mutt_cv_docdir
fi
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

AC_ARG_WITH(domain, AS_HELP_STRING([--with-domain=DOMAIN],[Specify your DNS domain name]),
	[if test $withval != yes; then
		if test $withval != no; then
			AC_DEFINE_UNQUOTED(DOMAIN,"$withval",[ Define your domain name. ])
		fi
	fi])

need_socket="no"

dnl -- socket dependencies --

dnl getaddrinfo/getaddrinfo_a is used by getdomain.c, and requires libnsl and
dnl libsocket on some platforms, and libanl
AC_CHECK_FUNC(gethostent, , AC_CHECK_LIB(nsl, gethostent))
AC_CHECK_FUNC(setsockopt, , AC_CHECK_LIB(socket, setsockopt))
AC_CHECK_FUNCS(getaddrinfo)
AC_SEARCH_LIBS([getaddrinfo_a], [anl], [AC_DEFINE(HAVE_GETADDRINFO_A, 1, [Define to 1 if you have the `getaddrinfo_a' function.])])

AS_IF([test x$use_pop = "xyes"], [
	AC_DEFINE(USE_POP, 1, [Define if you want support for the POP3 protocol.])
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS pop.o pop_lib.o pop_auth.o"
	need_pop="yes"
	need_socket="yes"
	need_md5="yes"
])

AS_IF([test x$use_imap = "xyes"], [
	AC_DEFINE(USE_IMAP, 1, [Define if you want support for the IMAP protocol.])
	LIBIMAP="-Limap -limap"
	LIBIMAPDEPS="\$(top_srcdir)/imap/imap.h imap/libimap.a"
	need_imap="yes"
	need_socket="yes"
	need_md5="yes"
])
AM_CONDITIONAL(BUILD_IMAP, test x$need_imap = xyes)

AS_IF([test x$use_nntp = "xyes"], [
	AC_DEFINE(USE_NNTP, 1, [Define if you want support for the NNTP protocol.])
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS nntp.o newsrc.o"
	need_nntp="yes"
	need_socket="yes"
])


AS_IF([test x$use_smtp = "xyes"], [
	AC_DEFINE(USE_SMTP, 1, [Include internal SMTP relay support])
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS smtp.o"
	need_socket="yes"
])

if test x"$need_imap" = xyes -o x"$need_pop" = xyes -o x"$need_nntp" = xyes; then
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS bcache.o"
fi

dnl -- end socket dependencies --

if test "$need_socket" = "yes"; then
	AC_CHECK_HEADERS([sys/select.h])
	AC_MSG_CHECKING([for socklen_t])
	AC_EGREP_HEADER(socklen_t, sys/socket.h, AC_MSG_RESULT([yes]),
		AC_MSG_RESULT([no])
		AC_DEFINE(socklen_t,int,
			[ Define to 'int' if <sys/socket.h> doesn't have it. ]))
	AC_DEFINE(USE_SOCKET,1,
		[ Include code for socket support. Set automatically if you enable POP3 or IMAP ])
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS account.o mutt_socket.o mutt_tunnel.o"
fi

dnl -- imap dependencies --

AC_ARG_WITH(gss, AS_HELP_STRING([--with-gss@<:@=PFX@:>@],[Compile in GSSAPI authentication for IMAP]),
	gss_prefix="$withval", gss_prefix="no")
if test "$gss_prefix" != "no"; then
	if test "$need_imap" = "yes"; then
		MUTT_AM_PATH_GSSAPI(gss_prefix)
		AC_MSG_CHECKING(GSSAPI implementation)
		AC_MSG_RESULT($GSSAPI_IMPL)
		if test "$GSSAPI_IMPL" = "none"; then
			AC_CACHE_SAVE
			AC_MSG_RESULT([GSSAPI libraries not found])
		fi
		if test "$GSSAPI_IMPL" = "Heimdal"; then
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

AC_ARG_WITH(ssl, AS_HELP_STRING([--with-ssl@<:@=PFX@:>@],[Enable TLS support using OpenSSL]),
[	if test "$with_ssl" != "no"; then
		if test "$need_socket" != "yes"; then
			AC_MSG_WARN([SSL support is only useful with POP, IMAP or SMTP support])
		else
			if test "$with_ssl" != "yes"; then
				LDFLAGS="$LDFLAGS -L$withval/lib"
				CPPFLAGS="$CPPFLAGS -I$withval/include"
			fi
			saved_LIBS="$LIBS"

			crypto_libs=""
			AC_CHECK_LIB(z, deflate, [crypto_libs=-lz])
			AC_CHECK_LIB(crypto, X509_STORE_CTX_new,
				[crypto_libs="-lcrypto $crypto_libs"],
				AC_MSG_ERROR([Unable to find SSL library]), [$crypto_libs])
			AC_CHECK_LIB(ssl, SSL_new,,
				AC_MSG_ERROR([Unable to find SSL library]), [$crypto_libs])

			LIBS="$LIBS $crypto_libs"
			AC_CHECK_FUNCS(RAND_status RAND_egd)
			AC_CHECK_DECLS([SSL_set_mode, SSL_MODE_AUTO_RETRY],,
				AC_MSG_ERROR([Unable to find decent SSL header]), [[#include <openssl/ssl.h>]])

			AC_CHECK_DECL([X509_V_FLAG_PARTIAL_CHAIN],
				AC_DEFINE(HAVE_SSL_PARTIAL_CHAIN,1,[ Define if OpenSSL supports partial chains. ]),,
				[[#include <openssl/x509_vfy.h>]])

			AC_DEFINE(USE_SSL,1,[ Define if you want support for SSL. ])
			AC_DEFINE(USE_SSL_OPENSSL,1,[ Define if you want support for SSL via OpenSSL. ])
			LIBS="$saved_LIBS"
			MUTTLIBS="$MUTTLIBS -lssl $crypto_libs"
			MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS mutt_ssl.o"
			need_ssl=yes
		fi
	fi
])

AC_ARG_WITH([gnutls], AS_HELP_STRING([--with-gnutls@<:@=PFX@:>@],[enable TLS support using gnutls]),
	[gnutls_prefix="$withval"], [gnutls_prefix="no"])
if test "$gnutls_prefix" != "no" && test x"$need_ssl" != xyes; then
	if test "$need_socket" != "yes"; then
		AC_MSG_WARN([SSL support is only useful with POP, IMAP or SMTP support])
	else
		if test "$gnutls_prefix" != "yes"; then
			LDFLAGS="$LDFLAGS -L$gnutls_prefix/lib"
			CPPFLAGS="$CPPFLAGS -I$gnutls_prefix/include"
		fi
		saved_LIBS="$LIBS"

		AC_CHECK_LIB(gnutls, gnutls_check_version,
			[dnl GNUTLS found
			AC_CHECK_DECLS([GNUTLS_VERIFY_DISABLE_TIME_CHECKS], [], [],
				[[#include <gnutls/x509.h>]])

			LIBS="$LIBS -lgnutls"
			AC_CHECK_FUNCS(gnutls_priority_set_direct)
			AC_CHECK_TYPES([gnutls_certificate_credentials_t,
				gnutls_certificate_status_t,
				gnutls_datum_t,
				gnutls_digest_algorithm_t,
				gnutls_session_t,
				gnutls_transport_ptr_t,
				gnutls_x509_crt_t], [], [], [[#include <gnutls/gnutls.h>]])

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

AC_ARG_WITH(sasl, AS_HELP_STRING([--with-sasl@<:@=PFX@:>@],[Use SASL network security library]),
	[
	if test "$with_sasl" != "no"; then
		if test "$need_socket" != "yes"; then
			AC_MSG_ERROR([SASL support is only useful with POP or IMAP support])
		fi

		if test "$with_sasl" != "yes"; then
			CPPFLAGS="$CPPFLAGS -I$with_sasl/include"
			LDFLAGS="$LDFLAGS -L$with_sasl/lib"
		fi

		saved_LIBS="$LIBS"
		LIBS=

		# OpenSolaris provides a SASL2 interface in libsasl
		sasl_libs="sasl2 sasl"
		AC_SEARCH_LIBS(sasl_encode64, [$sasl_libs],,
			AC_MSG_ERROR([could not find sasl lib]),)

		MUTTLIBS="$MUTTLIBS $LIBS"
		MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS mutt_sasl.o"
		LIBS="$saved_LIBS"

		AC_DEFINE(USE_SASL,1,
			[ Define if want to use the SASL library for POP/IMAP authentication. ])
		need_sasl=yes
	fi
	])
AM_CONDITIONAL(USE_SASL, test x$need_sasl = xyes)

dnl -- end socket --

AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug],[Enable debugging support]),
	[ if test x$enableval = xyes; then
		AC_DEFINE(DEBUG,1,[ Define to enable debugging info. ])
	fi
])

AC_ARG_ENABLE(flock, AS_HELP_STRING([--enable-flock],[Use flock() to lock files]),
	[if test $enableval = yes; then
		AC_DEFINE(USE_FLOCK,1, [ Define to use flock() to lock mailboxes. ])
	fi])

mutt_cv_fcntl=yes
AC_ARG_ENABLE(fcntl, AS_HELP_STRING([--disable-fcntl],[Do NOT use fcntl() to lock files]),
	[if test $enableval = no; then mutt_cv_fcntl=no; fi])

if test $mutt_cv_fcntl = yes; then
	AC_DEFINE(USE_FCNTL,1, [ Define to use fcntl() to lock folders. ])
fi

AC_MSG_CHECKING(whether struct dirent defines d_ino)
ac_cv_dirent_d_ino=no
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <dirent.h>]], [[struct dirent dp; (void)dp.d_ino]])],[ac_cv_dirent_d_ino=yes],[])
if test x$ac_cv_dirent_d_ino = xyes; then
	AC_DEFINE(HAVE_DIRENT_D_INO,1,
		[Define to 1 if your system has the dirent::d_ino member])
fi
AC_MSG_RESULT($ac_cv_dirent_d_ino)

mutt_cv_warnings=yes
AC_ARG_ENABLE(warnings, AS_HELP_STRING([--disable-warnings],[Turn off compiler warnings (not recommended)]),
	[if test $enableval = no; then
		mutt_cv_warnings=no
	fi])

if test x$GCC = xyes && test $mutt_cv_warnings = yes; then
	CFLAGS="-Wall -pedantic -Wno-long-long $CFLAGS"
fi

AC_ARG_ENABLE(nfs-fix, AS_HELP_STRING([--enable-nfs-fix],[Work around an NFS with broken attributes caching]),
	[if test x$enableval = xyes; then
		AC_DEFINE(NFS_ATTRIBUTE_HACK,1,
			[Define if you have problems with mutt not detecting
			new/old mailboxes over NFS.  Some NFS implementations
			incorrectly cache the attributes of small files.])
	fi])

AC_ARG_ENABLE(mailtool, AS_HELP_STRING([--enable-mailtool],[Enable Sun mailtool attachments support]),
	[if test x$enableval = xyes; then
		AC_DEFINE(SUN_ATTACHMENT,1,[ Define to enable Sun mailtool attachments support. ])
	fi])

AC_ARG_ENABLE(locales-fix, AS_HELP_STRING([--enable-locales-fix],[The result of isprint() is unreliable]),
	[if test x$enableval = xyes; then
		AC_DEFINE(LOCALES_HACK,1,[ Define if the result of isprint() is unreliable. ])
	fi])

AC_ARG_WITH(exec-shell, AS_HELP_STRING([--with-exec-shell=SHELL],[Specify alternate shell (ONLY if /bin/sh is broken)]),
	[if test $withval != yes; then
		AC_DEFINE_UNQUOTED(EXECSHELL, "$withval",
		[program to use for shell commands])
	else
		AC_DEFINE_UNQUOTED(EXECSHELL, "/bin/sh")
	fi],
	[AC_DEFINE_UNQUOTED(EXECSHELL, "/bin/sh")])

AC_ARG_ENABLE(exact-address, AS_HELP_STRING([--enable-exact-address],[Enable regeneration of email addresses]),
	[if test $enableval = yes; then
		AC_DEFINE(EXACT_ADDRESS,1,
			[Enable exact regeneration of email addresses as parsed?
			NOTE: this requires significant more memory when defined.])

	fi])

dnl -- start cache --
hcache_db_used=
AC_ARG_WITH(gdbm,
	AS_HELP_STRING(
		[--with-gdbm@<:@=DIR@:>@],
		[Use gdbm for the header cache]),
		[hcache_gdbm=$withval])
AC_ARG_WITH(tokyocabinet,
	AS_HELP_STRING(
		[--with-tokyocabinet@<:@=DIR@:>@],
		[Use tokyocabinet for the header cache]),
		[hcache_tokyocabinet=$withval])
AC_ARG_WITH(kyotocabinet,
	AS_HELP_STRING(
		[--with-kyotocabinet@<:@=DIR@:>@],
		[Use kyotocabinet for the header cache]),
		[hcache_kyotocabinet=$withval])
AC_ARG_WITH(qdbm,
	AS_HELP_STRING(
		[--with-qdbm@<:@=DIR@:>@],
		[Use qdbm for the header cache]),
		[hcache_qdbm=$withval])
AC_ARG_WITH(bdb,
	AS_HELP_STRING(
		[--with-bdb@<:@=DIR@:>@],
		[Use BerkeleyDB for the header cache]),
		[hcache_bdb=$withval])
AC_ARG_WITH(lmdb,
	AS_HELP_STRING(
		[--with-lmdb@<:@=DIR@:>@],
		[Use LMDB for the header cache]),
		[hcache_lmdb=$withval])

dnl -- Tokyo Cabinet --
if test -n "$hcache_tokyocabinet" && test "$hcache_tokyocabinet" != "no"; then
	OLDCPPFLAGS="$CPPFLAGS"
	OLDLDFLAGS="$LDFLAGS"
	if test "$hcache_tokyocabinet" != "yes"; then
		CPPFLAGS="$CPPFLAGS -I$hcache_tokyocabinet/include"
		LDFLAGS="$LDFLAGS -L$hcache_tokyocabinet/lib"
	fi

	AC_CHECK_HEADER(tcbdb.h,
	AC_CHECK_LIB(tokyocabinet, tcbdbopen,
		[
			MUTTLIBS="$MUTTLIBS -ltokyocabinet"
			AC_DEFINE(HAVE_TC, 1, [Tokyo Cabinet Support])
			MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS hcache_tc.o"
			hcache_db_used="tokyocabinet $hcache_db_used"
		],[
			CPPFLAGS="$OLDCPPFLAGS"
			LDFLAGS="$OLDLDFLAGS"
			AC_MSG_ERROR(Unable to find TokyoCabinet)
		]),	AC_MSG_ERROR(Unable to find TokyoCabinet))
fi

dnl -- Kyoto Cabinet --
if test -n "$hcache_kyotocabinet" && test "$hcache_kyotocabinet" != "no"; then
	OLDCPPFLAGS="$CPPFLAGS"
	OLDLDFLAGS="$LDFLAGS"
	if test "$hcache_kyotocabinet" != "yes"; then
		CPPFLAGS="$CPPFLAGS -I$hcache_kyotocabinet/include"
		LDFLAGS="$LDFLAGS -L$hcache_kyotocabinet/lib"
	fi

	AC_CHECK_HEADER(kclangc.h,
	AC_CHECK_LIB(kyotocabinet, kcdbopen,
		[
			MUTTLIBS="$MUTTLIBS -lkyotocabinet"
			AC_DEFINE(HAVE_KC, 1, [Kyoto Cabinet Support])
			MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS hcache_kc.o"
			hcache_db_used="kyotocabinet $hcache_db_used"
		],[
			CPPFLAGS="$OLDCPPFLAGS"
			LDFLAGS="$OLDLDFLAGS"
			AC_MSG_ERROR(Unable to find KyotoCabinet)
		]),	AC_MSG_ERROR(Unable to find KyotoCabinet))
fi

dnl -- GDBM --
dnl -- Make sure the GDBM block comes before the QDBM one. QDBM provides
dnl -- the GDBM symbols in a compatibility layer (google for gdbm hovel).
dnl -- By doing this, we make sure the symbols are resolved in GDBM's
dnl -- library when both GDBM and QDBM are linked.
if test -n "$hcache_gdbm" && test "$hcache_gdbm" != "no"; then
	OLDCPPFLAGS="$CPPFLAGS"
	OLDLDFLAGS="$LDFLAGS"
	if test "$hcache_gdbm" != "yes"; then
		CPPFLAGS="$CPPFLAGS -I$hcache_gdbm/include"
		LDFLAGS="$LDFLAGS -L$hcache_gdbm/lib"
	fi

	AC_CHECK_HEADERS(gdbm.h,
	AC_CHECK_LIB(gdbm, gdbm_open,
		[
			MUTTLIBS="$MUTTLIBS -lgdbm"
			MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS hcache_gdbm.o"
			AC_DEFINE(HAVE_GDBM, 1, [GDBM Support])
			hcache_db_used="gdbm $hcache_db_used"
		],[
			CPPFLAGS="$OLDCPPFLAGS"
			LDFLAGS="$OLDLDFLAGS"
			AC_MSG_ERROR(Unable to find GDBM)
		]),	AC_MSG_ERROR(Unable to find GDBM))
fi

dnl -- QDBM --
if test -n "$hcache_qdbm" && test "$hcache_qdbm" != "no"; then
	OLDCPPFLAGS="$CPPFLAGS"
	OLDLDFLAGS="$LDFLAGS"
	if test "$hcache_qdbm" != "yes"; then
		if test -d $hcache_qdbm/include/qdbm; then
			CPPFLAGS="$CPPFLAGS -I$hcache_qdbm/include/qdbm"
		else
			CPPFLAGS="$CPPFLAGS -I$hcache_qdbm/include"
		fi
		LDFLAGS="$LDFLAGS -L$hcache_qdbm/lib"
	else
		if test -d /usr/include/qdbm; then
			CPPFLAGS="$CPPFLAGS -I/usr/include/qdbm"
		fi
	fi

	AC_CHECK_HEADERS(villa.h,
	AC_CHECK_LIB(qdbm, vlopen,
		[
			MUTTLIBS="$MUTTLIBS -lqdbm"
			MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS hcache_qdbm.o"
			AC_DEFINE(HAVE_QDBM, 1, [QDBM Support])
			hcache_db_used="qdbm $hcache_db_used"
		],[
			CPPFLAGS="$OLDCPPFLAGS"
			LDFLAGS="$OLDLDFLAGS"
			AC_MSG_ERROR(Unable to find QDBM)
		]),	AC_MSG_ERROR(Unable to find QDBM))
fi

dnl -- BDB --
ac_bdb_prefix="$mutt_cv_prefix /opt /usr/local /usr"
if test -n "$hcache_bdb" && test "$hcache_bdb" != "no"; then
	OLDCPPFLAGS="$CPPFLAGS"
	OLDLDFLAGS="$LDFLAGS"
	if test "$hcache_bdb" != "yes"; then
		ac_bdb_prefix="$hcache_bdb $mutt_cv_prefix"
	fi
	BDB_VERSIONS="db-5.3 db53 db-5 db5 db-4.8 db48 db-4 db4"
	for d in $ac_bdb_prefix; do
		for v in / $BDB_VERSIONS; do
			AC_MSG_CHECKING([for BerkeleyDB in $d/include/$v])
			if test -r "$d/include/$v/db.h"; then
				BDB_INCLUDE_DIR="$d/include/$v"
				BDB_INCLUDE_FILE="$d/include/$v/db.h"
				BDB_VERSION_MAJOR=$(awk '/DB_VERSION_MAJOR/{print $NF}' \
						$BDB_INCLUDE_FILE)
				BDB_VERSION_MINOR=$(awk '/DB_VERSION_MINOR/{print $NF}' \
						$BDB_INCLUDE_FILE)
				if test -z "$BDB_VERSION_MAJOR" || \
					test -z "$BDB_VERSION_MINOR"; then
						AC_MSG_RESULT(yes, missing DB_VERSION_MAJOR / DB_VERSION_MINOR)
						continue
				fi
				AC_MSG_RESULT(yes)
				BDB_LIB_DIR="$d/lib/$v"
				BDB_LIB_NAME="db-$BDB_VERSION_MAJOR.$BDB_VERSION_MINOR"
				CPPFLAGS="$OLDCPPFLAGS -I$BDB_INCLUDE_DIR"
				LDFLAGS="$OLDLDFLAGS -L$BDB_LIB_DIR"
				AC_CHECK_LIB($BDB_LIB_NAME, db_env_create,
					[
						MUTTLIBS="$MUTTLIBS -l$BDB_LIB_NAME"
						MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS hcache_bdb.o"
						AC_DEFINE(HAVE_BDB, 1, [Berkeley DB Support])
						hcache_db_used="bdb $hcache_db_used"
						BDB_FOUND=yes
						break
				],[
					CPPFLAGS="$OLDCPPFLAGS"
					LDFLAGS="$OLDLDFLAGS"
				])
			else
				AC_MSG_RESULT(no)
			fi
		done
		test "$BDB_FOUND" = "yes" && break
	done
	if test "$BDB_FOUND" != "yes"; then
		AC_MSG_ERROR(Unable to find BDB)
	fi
fi

dnl -- LMDB --
if test -n "$hcache_lmdb" && test "$hcache_lmdb" != "no"; then
	OLDCPPFLAGS="$CPPFLAGS"
	OLDLDFLAGS="$LDFLAGS"
	if test "$hcache_lmdb" != "yes"; then
		CPPFLAGS="$CPPFLAGS -I$hcache_lmdb/include"
		LDFLAGS="$LDFLAGS -L$hcache_lmdb/lib"
	fi
	AC_CHECK_HEADERS(lmdb.h,
	AC_CHECK_LIB(lmdb, mdb_env_create,
		[
			AC_DEFINE(HAVE_LMDB, 1, [LMDB Support])
			MUTTLIBS="$MUTTLIBS -llmdb"
			MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS hcache_lmdb.o"
			hcache_db_used="lmdb $hcache_db_used"
		],[
			CPPFLAGS="$OLDCPPFLAGS"
			LDFLAGS="$OLDLDFLAGS"
			AC_MSG_ERROR(Unable to find LMDB)
		]),	AC_MSG_ERROR(Unable to find LMDB))
fi

AM_CONDITIONAL(BUILD_HCACHE, test -n "$hcache_db_used")
if test -n "$hcache_db_used"; then
	AC_DEFINE(USE_HCACHE, 1, [Enable header caching])
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS hcache.o"
	need_md5="yes"
else
	# For outputting in the summary
	hcache_db_used="no"
fi
dnl -- end cache --

if test "$need_md5" = "yes"; then
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS md5.o"
	MUTT_MD5="mutt_md5$EXEEXT"
fi

AC_SUBST(MUTT_MD5)
AC_SUBST(MUTTLIBS)
AC_SUBST(MUTT_LIB_OBJECTS)
AC_SUBST(LIBIMAP)
AC_SUBST(LIBIMAPDEPS)
AC_SUBST(NOTMUCH_LIBS)

dnl -- iconv/gettext --

AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])
AC_CHECK_HEADERS([locale.h])

AM_ICONV
if test "$am_cv_func_iconv" != yes; then
    AC_MSG_ERROR([An iconv implementation is required])
fi

dnl (1) Some implementations of iconv won't convert from UTF-8 to UTF-8.
dnl (2) In glibc-2.1.2 and earlier there is a bug that messes up ob and
dnl     obl when args 2 and 3 are 0 (fixed in glibc-2.1.3).
AC_CACHE_CHECK([whether this iconv is good enough], mutt_cv_iconv_good,
	mutt_save_LIBS="$LIBS"
	LIBS="$LIBS $LIBICONV"
	AC_RUN_IFELSE([AC_LANG_SOURCE([[
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
]])],[mutt_cv_iconv_good=yes],[mutt_cv_iconv_good=no],[mutt_cv_iconv_good=yes])
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
	AC_RUN_IFELSE([AC_LANG_SOURCE([[
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
]])],[mutt_cv_iconv_nontrans=no],[mutt_cv_iconv_nontrans=yes],[mutt_cv_iconv_nontrans=no])
	LIBS="$mutt_save_LIBS")
if test "$mutt_cv_iconv_nontrans" = yes; then
	AC_DEFINE(ICONV_NONTRANS, 1)
else
	AC_DEFINE(ICONV_NONTRANS, 0)
fi

AC_ARG_WITH(idn, AS_HELP_STRING([--with-idn=@<:@PFX@:>@],[Use GNU libidn for internationalized domain names]),
		[
		if test "$with_idn" != "no"; then
			if test "$with_idn" != "yes"; then
				CPPFLAGS="$CPPFLAGS -I$with_idn/include"
				LDFLAGS="$LDFLAGS -L$with_idn/lib"
			fi
		fi
		],
		[with_idn=auto])

if test "x$with_idn" != "xno"; then
    dnl Solaris 11 has /usr/include/idn
    have_stringprep_h=no
    AC_CHECK_HEADERS([stringprep.h idn/stringprep.h], [
                have_stringprep_h=yes
                break])
    have_idna_h=no
    AC_CHECK_HEADERS([idna.h idn/idna.h], [
                have_idna_h=yes
                break])

    mutt_save_LIBS="$LIBS"
    LIBS=

    AC_SEARCH_LIBS([stringprep_check_version], [idn], [
        AC_DEFINE([HAVE_LIBIDN], 1, [Define to 1 if you have the GNU idn library])
        MUTTLIBS="$MUTTLIBS $LIBS"

        LIBS="$LIBS $LIBICONV"
        AC_CHECK_FUNCS(idna_to_unicode_utf8_from_utf8 idna_to_unicode_8z8z)
        AC_CHECK_FUNCS(idna_to_ascii_from_utf8 idna_to_ascii_8z)
        AC_CHECK_FUNCS(idna_to_ascii_lz idna_to_ascii_from_locale)
    ])

    LIBS="$mutt_save_LIBS"

    if test "$with_idn" != auto; then
        if test $have_stringprep_h = no || test $have_idna_h = no || test $ac_cv_search_stringprep_check_version = no; then
            AC_MSG_ERROR([IDN was requested, but libidn was not usable on this system])
        fi
	fi
fi

dnl -- locales --

AC_CHECK_HEADERS(wchar.h)

AC_CACHE_CHECK([for wchar_t], mutt_cv_wchar_t,
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <stddef.h>
#include <stdlib.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
]], [[ wchar_t wc; return 0; ]])],[mutt_cv_wchar_t=yes],[mutt_cv_wchar_t=no]))

if test "$mutt_cv_wchar_t" = no; then
	AC_DEFINE(wchar_t,int,[ Define to 'int' if system headers don't define. ])
fi

AC_CACHE_CHECK([for wint_t], mutt_cv_wint_t,
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <stddef.h>
#include <stdlib.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
]], [[ wint_t wc; return 0; ]])],[mutt_cv_wint_t=yes],[mutt_cv_wint_t=no]))

if test "$mutt_cv_wint_t" = no; then
	AC_DEFINE(wint_t,int,[ Define to 'int' if system headers don't define. ])
fi

AC_CHECK_HEADERS(wctype.h)
AC_CHECK_FUNCS(iswalnum iswalpha iswblank iswcntrl iswdigit)
AC_CHECK_FUNCS(iswgraph iswlower iswprint iswpunct iswspace iswupper)
AC_CHECK_FUNCS(iswxdigit towupper towlower)

AC_CACHE_CHECK([for mbstate_t], mutt_cv_mbstate_t,
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <stddef.h>
#include <stdlib.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
]], [[ mbstate_t s; return 0; ]])],[mutt_cv_mbstate_t=yes],[mutt_cv_mbstate_t=no]))

if test "$mutt_cv_mbstate_t" = no; then
	AC_DEFINE(mbstate_t,int,[ Define to 'int' if system headers don't define. ])
fi

wc_funcs=maybe
AC_ARG_WITH(wc-funcs, AS_HELP_STRING([--without-wc-funcs],[Do not use the system's wchar_t functions]),
	wc_funcs=$withval)

if test "$wc_funcs" != yes && test "$wc_funcs" != no; then
	AC_CACHE_CHECK([for wchar_t functions], mutt_cv_wc_funcs,
		mutt_cv_wc_funcs=no
		AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#define _XOPEN_SOURCE 600
#include <stddef.h>
#include <stdlib.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#endif]], [[mbrtowc(0, 0, 0, 0); wctomb(0, 0); wcwidth(0);
	iswprint(0); iswspace(0); towlower(0); towupper(0); iswalnum(0)]])],[mutt_cv_wc_funcs=yes],[]))
	wc_funcs=$mutt_cv_wc_funcs
fi

if test $wc_funcs = yes; then
	AC_DEFINE(HAVE_WC_FUNCS,1,[ Define if you are using the system's wchar_t functions. ])
else
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS utf8.o wcwidth.o"
fi

AC_CACHE_CHECK([for nl_langinfo and CODESET], mutt_cv_langinfo_codeset,
	[AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <langinfo.h>]], [[char* cs = nl_langinfo(CODESET);]])],[mutt_cv_langinfo_codeset=yes],[mutt_cv_langinfo_codeset=no])])
if test $mutt_cv_langinfo_codeset = yes; then
	AC_DEFINE(HAVE_LANGINFO_CODESET,1,[ Define if you have <langinfo.h> and nl_langinfo(CODESET). ])
fi

AC_CACHE_CHECK([for nl_langinfo and YESEXPR], mutt_cv_langinfo_yesexpr,
	[AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <langinfo.h>]], [[char* cs = nl_langinfo(YESEXPR);]])],[mutt_cv_langinfo_yesexpr=yes],[mutt_cv_langinfo_yesexpr=no])])
if test $mutt_cv_langinfo_yesexpr = yes; then
	AC_DEFINE(HAVE_LANGINFO_YESEXPR,1,[ Define if you have <langinfo.h> and nl_langinfo(YESEXPR). ])
fi

# Only enable fmemopen if both fmemopen() and open_memstream()
# AND --enable-fmemopen is given.
have_fmemopen=yes
AC_CHECK_FUNCS(fmemopen open_memstream, [], [have_fmemopen=no])

AC_ARG_ENABLE(fmemopen, AS_HELP_STRING([--enable-fmemopen],[Use fmemopen]),
	[use_fmemopen=$enableval], [use_fmemopen=no]
)

AS_IF([test $have_fmemopen = "yes" && test $use_fmemopen = "yes"],
	# Temporarily disable fmemopen, due to a bug
	[AC_DEFINE(USE_FMEMOPEN, 0, [Use fmemopen])],
	[use_fmemopen=no]
)

dnl Documentation tools
have_openjade="no"
AC_PATH_PROG([OSPCAT], [ospcat], [none])
if test "$OSPCAT" != "none"; then
	AC_MSG_CHECKING([for openjade docbook stylesheets])
	dslosfile=`ospcat --public-id="-//Norman Walsh//DOCUMENT DocBook Print Stylesheet//EN"`
	DSLROOT=`echo $dslosfile | sed -n -e "s/.*SOIBASE='\(@<:@^'@:>@*\)\/catalog'.*/\1/p"`
	# ospcat may spit out an absolute path without an SOIBASE
	if test -z "$DSLROOT"; then
		DSLROOT=`echo $dslosfile | sed -e 's|<OSFILE>\(.*\)/print/docbook.dsl|\1|'`
	fi
	if test -f $DSLROOT/print/docbook.dsl; then
		AC_MSG_RESULT([in $DSLROOT])
		have_openjade="yes"
	else
		AC_MSG_RESULT([not found: PDF documentation will not be built.])
	fi
fi
AC_SUBST(DSLROOT)

AC_ARG_ENABLE(doc, AS_HELP_STRING([--disable-doc],[Do not build the documentation]),
[	if test x$enableval = xno; then
		do_build_doc=no
	fi
])
AM_CONDITIONAL(BUILD_DOC, test x$do_build_doc != xno)

AC_ARG_ENABLE(po, AS_HELP_STRING([--disable-po],[Do not build the translation messages]),
[	if test x$enableval = xno; then
		do_build_po=no
	fi
])
AM_CONDITIONAL(BUILD_PO, test x$do_build_po != xno)

AC_ARG_ENABLE(full_doc,
	AS_HELP_STRING([--disable-full-doc],[Omit disabled variables]),
[	if test x$enableval = xno; then
		full_doc=no
	fi
])
if test x$full_doc != xno; then
	AC_DEFINE(MAKEDOC_FULL,1, [Define if you want complete documentation.])
fi

AC_CONFIG_FILES(Makefile contrib/Makefile doc/Makefile imap/Makefile
	m4/Makefile po/Makefile.in
	hcachever.sh doc/instdoc.sh)
AC_OUTPUT

AC_MSG_NOTICE([Summary of build options:

  Version:           ${PACKAGE_VERSION}
  Host OS:           ${host_os}
  Install prefix:    ${prefix}
  Compiler:          ${CC}
  CFlags:            ${CFLAGS} ${CPPFLAGS}
  LDFlags:           ${LDFLAGS}
  Libs:              ${LIBS}
  Mutt libs:         ${MUTTLIBS}

  GPGME:             $use_gpgme
  PGP:               $use_pgp
  SMIME:             $use_smime
  Sidebar:           $use_sidebar
  Notmuch:           $use_notmuch
  Compressed Folder: $use_compressed
  Header Cache(s):   $hcache_db_used
  Lua:               $use_lua

  POP3:              $use_pop
  IMAP:              $use_imap
  NNTP:              $use_nntp
  SMTP:              $use_smtp
])
