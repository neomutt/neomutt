dnl To create the configure script, run:
dnl     autoreconf -i

CFLAGS=$CFLAGS
LDFLAGS=$LDFLAGS

AC_INIT([NeoMutt], [20170714], [neomutt-devel@neomutt.org], [mutt], [https://www.neomutt.org])
AC_CONFIG_SRCDIR(mutt.h)
AC_CONFIG_AUX_DIR([.build-aux])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

AC_DEFINE(MUTT_VERSION, "1.8.3", [Version of upstream Mutt])

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

CFLAGS="-Wall -pedantic -Wno-long-long $CFLAGS"

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

AC_C_BIGENDIAN

AC_SYS_LARGEFILE
AC_CHECK_SIZEOF(off_t)

AH_TEMPLATE([sig_atomic_t],
	[Define to `int' if <signal.h> doesn't define.])
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
#define LOFF_T off_t
#if SIZEOF_OFF_T == 8
# define OFF_T_FMT "%" PRId64
#else
# define OFF_T_FMT "%" PRId32
#endif
])

ac_aux_path_sendmail=/usr/sbin:/usr/lib
AC_PATH_PROG(SENDMAIL, sendmail, /usr/sbin/sendmail, $PATH:$ac_aux_path_sendmail)
AC_DEFINE_UNQUOTED(SENDMAIL,"$ac_cv_path_SENDMAIL", [Where to find sendmail on your system.])

OPS='$(srcdir)/OPS $(srcdir)/OPS.SIDEBAR'

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
	use_notmuch="yes"
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
	use_notmuch="no"
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

AC_ARG_ENABLE(lua,
	AC_HELP_STRING([--enable-lua], [Enable lua scripting support]),
	[use_lua=$enableval])

AC_ARG_ENABLE(notmuch,
	AC_HELP_STRING([--enable-notmuch], [Enable NOTMUCH support]),
	[use_notmuch=$enableval])

AC_ARG_WITH(mixmaster,
	AS_HELP_STRING([--with-mixmaster@<:@=PATH@:>@], [Include Mixmaster support]),
	[alongwith_mixmaster=$withval], [alongwith_mixmaster=no])

dnl == End of Features and Options Definitions ==

dnl == Declare all the checks and code for options / features below this line ==

dnl --enable-gpgme
AS_IF([test x$use_gpgme = "xyes"], [
	ifdef([AM_PATH_GPGME], [
		AM_PATH_GPGME(1.1.0, [
			AC_DEFINE(CRYPT_BACKEND_GPGME, 1, [GPGME support])
			AC_CHECK_FUNCS([gpgme_op_export_keys])
			build_ncrypt_gpgme="yes"
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
	build_ncrypt_pgp="yes"
])

dnl --enable-smime
AS_IF([test x$use_smime != "xno"], [
	AC_DEFINE(CRYPT_BACKEND_CLASSIC_SMIME, 1, [Define if you want classic S/MIME support.])
	SMIMEAUX_TARGET='$(top_srcdir)/contrib/smime_keys'
	build_ncrypt_smime="yes"
])

dnl Ideally, these two defines should be dependent on :
dnl     $build_ncrypt_gpgme, $build_ncrypt_pgp, $build_ncrypt_smime
dnl but there are two many places in the code that still reference the crypto code.
NCRYPT_LIBS="-Lncrypt -lncrypt"
NCRYPT_DEPS="ncrypt/libncrypt.a"
AC_SUBST(NCRYPT_LIBS)
AC_SUBST(NCRYPT_DEPS)

AM_CONDITIONAL(BUILD_NCRYPT_GPGME, test "x$build_ncrypt_gpgme" = "xyes")
AM_CONDITIONAL(BUILD_NCRYPT_PGP,   test "x$build_ncrypt_pgp"   = "xyes")
AM_CONDITIONAL(BUILD_NCRYPT_SMIME, test "x$build_ncrypt_smime" = "xyes")

AC_DEFINE(USE_COMPRESSED, 1, [Define to enable compressed folders support.])
AC_DEFINE(USE_IMAP, 1, [Define if you want support for the IMAP protocol.])
AC_DEFINE(USE_NNTP, 1, [Define if you want support for the NNTP protocol.])
AC_DEFINE(USE_POP, 1, [Define if you want support for the POP3 protocol.])
AC_DEFINE(USE_SIDEBAR, 1, [Define if you want support for the sidebar.])
AC_DEFINE(USE_SMTP, 1, [Include internal SMTP relay support])
AC_DEFINE(USE_SOCKET,1,
	[ Include code for socket support. Set automatically if you enable POP3 or IMAP ])
AC_DEFINE(SUN_ATTACHMENT,1,[ Define to enable Sun mailtool attachments support. ])

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

dnl --enable-notmuch
AS_IF([test x$use_notmuch = "xyes"], [
	AC_CHECK_LIB(notmuch, notmuch_database_open, [:],
		AC_MSG_ERROR([Unable to find Notmuch library]))
	AC_DEFINE(USE_NOTMUCH,1,[ Define if you want support for the notmuch. ])
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS mutt_notmuch.o"
	LIBS="$LIBS -lnotmuch"
	OPS="$OPS \$(srcdir)/OPS.NOTMUCH"

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

want_slang=no
AC_ARG_WITH(slang, AS_HELP_STRING([--with-slang@<:@=DIR@:>@],[Use S-Lang instead of ncurses]),
	[want_slang=$withval])
if test $want_slang != no; then
	withval=$want_slang
	AC_CACHE_CHECK([if this is a BSD system], mutt_cv_bsdish,
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
		dnl -- Check to see if $withval is a source directory
		if test -f $withval/src/slang.h; then
			mutt_cv_slang=$withval/src
			CPPFLAGS="$CPPFLAGS -I${mutt_cv_slang}"
			LDFLAGS="$LDFLAGS -L${mutt_cv_slang}/objs"
		else
			dnl -- Must be installed somewhere
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
		[LIBS="$LIBS -lslang -lm"],
		[AC_MSG_ERROR(unable to compile.  check config.log)], -lm)
else
	mutt_cv_curses=/usr
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
		for lib in ncursesw ncurses curses
		do
			AC_CHECK_LIB($lib, waddnwstr, [cf_ncurses="$lib"; break])
		done
		if test -z $cf_ncurses; then
			AC_MSG_ERROR([Unable to find ncursesw library])
		fi
		AC_CHECK_LIB($cf_ncurses, initscr,[
			LIBS="$LIBS -l$cf_ncurses"
			AC_CHECK_LIB($cf_ncurses, tgetent, [:], [
				AC_CHECK_LIB(tinfo, tgetent, [LIBS="$LIBS -ltinfo"])
			])

			AC_CHECK_HEADERS(ncursesw/ncurses.h,
				[cf_cv_ncurses_header="ncursesw/ncurses.h"],
				AC_CHECK_HEADERS(ncurses/ncurses.h,
					[cf_cv_ncurses_header="ncurses/ncurses.h"],
					AC_CHECK_HEADERS(ncurses.h,
						[cf_cv_ncurses_header="ncurses.h"],
						AC_MSG_ERROR(Unable to find ncurses headers)
					)
				)
			)
		],)
	])

	CF_CHECK_FUNCDECLS([#include <${cf_cv_ncurses_header-curses.h}>],
		[start_color typeahead bkgdset curs_set meta use_default_colors resizeterm])
	if test "$ac_cv_func_decl_start_color" = yes; then
		AC_DEFINE(HAVE_COLOR,1,[ Define if your curses library supports color. ])
	fi
	if test "$ac_cv_func_decl_resizeterm" = yes; then
		MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS resize.o"
	fi
	AC_CHECK_FUNCS([use_extended_names])
fi

AC_HEADER_STDC

AC_CHECK_HEADERS(sys/ioctl.h ioctl.h sysexits.h)
AC_CHECK_HEADERS(sys/syscall.h)

AC_CHECK_FUNCS(fgets_unlocked fgetc_unlocked)
AC_CHECK_FUNCS(strsep mkdtemp)

AC_MSG_CHECKING(for sig_atomic_t in signal.h)
AC_EGREP_HEADER(volatile.*sig_atomic_t,signal.h,
	[
		AC_MSG_RESULT([yes, volatile])
		AC_DEFINE(SIG_ATOMIC_VOLATILE_T, sig_atomic_t)
	], [
		AC_MSG_RESULT([yes, non volatile])
		AC_DEFINE(SIG_ATOMIC_VOLATILE_T, [volatile sig_atomic_t])
	])

AC_CHECK_DECLS([sys_siglist],[],[],[#include <signal.h>
/* NetBSD declares sys_siglist in unistd.h.  */
# include <unistd.h>
])

AC_REPLACE_FUNCS([wcscasecmp])

dnl Set the atime of files
AC_CHECK_FUNCS(futimens)

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
else
	AC_ARG_WITH(mailpath, AS_HELP_STRING([--with-mailpath=DIR],[Directory where spool mailboxes are located]),
		[mutt_cv_mailpath=$withval], [mutt_cv_mailpath=/var/mail])
	AC_DEFINE_UNQUOTED(MAILPATH,"$mutt_cv_mailpath",[ Where new mail is spooled. ])
fi

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

AC_ARG_WITH(domain, AS_HELP_STRING([--with-domain=DOMAIN],[Specify your DNS domain name]),
	[if test $withval != yes; then
		if test $withval != no; then
			AC_DEFINE_UNQUOTED(DOMAIN,"$withval",[ Define your domain name. ])
		fi
	fi])

dnl -- socket dependencies --

dnl getaddrinfo/getaddrinfo_a is used by getdomain.c, and requires libnsl and
dnl libsocket on some platforms, and libanl
AC_CHECK_FUNC(gethostent, , AC_CHECK_LIB(nsl, gethostent))
AC_CHECK_FUNC(setsockopt, , AC_CHECK_LIB(socket, setsockopt))
AC_CHECK_FUNCS(getaddrinfo)
AC_SEARCH_LIBS([getaddrinfo_a], [anl], [AC_DEFINE(HAVE_GETADDRINFO_A, 1, [Define to 1 if you have the `getaddrinfo_a' function.])])

dnl -- end socket dependencies --

dnl -- imap dependencies --

AC_ARG_WITH(gss, AS_HELP_STRING([--with-gss@<:@=PFX@:>@],[Compile in GSSAPI authentication for IMAP]),
	gss_prefix="$withval", gss_prefix="no")
if test "$gss_prefix" != "no"; then
	MUTT_AM_PATH_GSSAPI(gss_prefix)
	AC_MSG_CHECKING(GSSAPI implementation)
	AC_MSG_RESULT($GSSAPI_IMPL)
	if test "$GSSAPI_IMPL" = "none"; then
		AC_CACHE_SAVE
		AC_MSG_RESULT([GSSAPI libraries not found])
	fi
	if test "$GSSAPI_IMPL" = "Heimdal" || test "$GSSAPI_IMPL" = "Solaris"; then
		AC_DEFINE(HAVE_HEIMDAL,1,[ Define if your GSSAPI implementation is Heimdal ])
	fi
	CPPFLAGS="$CPPFLAGS $GSSAPI_CFLAGS"
	LIBS="$LIBS $GSSAPI_LIBS"
	AC_DEFINE(USE_GSS,1,[ Define if you have GSSAPI libraries available ])
	need_gss="yes"
fi
AM_CONDITIONAL(USE_GSS, test x$need_gss = xyes)

dnl -- end imap dependencies --

AC_ARG_WITH(ssl, AS_HELP_STRING([--with-ssl@<:@=PFX@:>@],[Enable TLS support using OpenSSL]),
[	if test "$with_ssl" != "no"; then
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

		LIBS="$saved_LIBS -lssl $crypto_libs"
		AC_CHECK_FUNCS(RAND_status RAND_egd)
		AC_CHECK_DECLS([SSL_set_mode, SSL_MODE_AUTO_RETRY],,
			AC_MSG_ERROR([Unable to find decent SSL header]), [[#include <openssl/ssl.h>]])

		AC_CHECK_DECL([X509_V_FLAG_PARTIAL_CHAIN],
			AC_DEFINE(HAVE_SSL_PARTIAL_CHAIN,1,[ Define if OpenSSL supports partial chains. ]),,
			[[#include <openssl/x509_vfy.h>]])

		AC_DEFINE(USE_SSL,1,[ Define if you want support for SSL. ])
		AC_DEFINE(USE_SSL_OPENSSL,1,[ Define if you want support for SSL via OpenSSL. ])
		MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS mutt_ssl.o"
		need_ssl=yes
	fi
])

AC_ARG_WITH([gnutls], AS_HELP_STRING([--with-gnutls@<:@=PFX@:>@],[enable TLS support using gnutls]),
	[gnutls_prefix="$withval"], [gnutls_prefix="no"])
if test "$gnutls_prefix" != "no" && test x"$need_ssl" != xyes; then
	if test "$gnutls_prefix" != "yes"; then
		LDFLAGS="$LDFLAGS -L$gnutls_prefix/lib"
		CPPFLAGS="$CPPFLAGS -I$gnutls_prefix/include"
	fi

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

		AC_DEFINE(USE_SSL, 1, [ Define if you want support for SSL. ])
		AC_DEFINE(USE_SSL_GNUTLS, 1, [ Define if you want support for SSL via GNUTLS. ])

		MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS mutt_ssl_gnutls.o"
		need_ssl=yes],
		[AC_MSG_ERROR([could not find libgnutls])])
fi

AC_ARG_WITH(sasl, AS_HELP_STRING([--with-sasl@<:@=PFX@:>@],[Use SASL network security library]),
	[
	if test "$with_sasl" != "no"; then
		if test "$with_sasl" != "yes"; then
			CPPFLAGS="$CPPFLAGS -I$with_sasl/include"
			LDFLAGS="$LDFLAGS -L$with_sasl/lib"
		fi

		# OpenSolaris provides a SASL2 interface in libsasl
		sasl_libs="sasl2 sasl"
		AC_SEARCH_LIBS(sasl_encode64, [$sasl_libs],,
			AC_MSG_ERROR([could not find sasl lib]),)

		MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS mutt_sasl.o"

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

AC_ARG_ENABLE(locales-fix, AS_HELP_STRING([--enable-locales-fix],[The result of isprint() is unreliable]),
	[if test x$enableval = xyes; then
		AC_DEFINE(LOCALES_HACK,1,[ Define if the result of isprint() is unreliable. ])
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
			HCACHE_LIBS="$HCACHE_LIBS -ltokyocabinet"
			AC_DEFINE(HAVE_TC, 1, [Tokyo Cabinet Support])
			hcache_db_used="tokyocabinet $hcache_db_used"
			build_hc_tc="yes"
		],[
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
			HCACHE_LIBS="$HCACHE_LIBS -lkyotocabinet"
			AC_DEFINE(HAVE_KC, 1, [Kyoto Cabinet Support])
			hcache_db_used="kyotocabinet $hcache_db_used"
			build_hc_kc="yes"
		],[
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
			HCACHE_LIBS="$HCACHE_LIBS -lgdbm"
			AC_DEFINE(HAVE_GDBM, 1, [GDBM Support])
			hcache_db_used="gdbm $hcache_db_used"
			build_hc_gdbm="yes"
		],[
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
			HCACHE_LIBS="$HCACHE_LIBS -lqdbm"
			AC_DEFINE(HAVE_QDBM, 1, [QDBM Support])
			hcache_db_used="qdbm $hcache_db_used"
			build_hc_qdbm="yes"
		],[
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
	BDB_VERSIONS="$BDB_VERSIONS db-5.3 db53 db5.3 db-5 db5"
	BDB_VERSIONS="$BDB_VERSIONS db-6.2 db62 db6.2 db-6 db6"
	BDB_VERSIONS="$BDB_VERSIONS db-4.8 db48 db4.8 db-4 db4"
	for d in $ac_bdb_prefix; do
		for v in / $BDB_VERSIONS; do
			AC_MSG_CHECKING([for BerkeleyDB in $d/include/$v])
			if test -r "$d/include/$v/db.h"; then
				BDB_INCLUDE_DIR="$d/include/$v"
				BDB_INCLUDE_FILE="$d/include/$v/db.h"
				AC_COMPUTE_INT(BDB_VERSION_MAJOR, DB_VERSION_MAJOR,
					[#include "$BDB_INCLUDE_FILE"], continue)
				AC_COMPUTE_INT(BDB_VERSION_MINOR, DB_VERSION_MINOR,
					[#include "$BDB_INCLUDE_FILE"], continue)
				AC_MSG_RESULT(yes)
				BDB_LIB_DIR="$d/lib/$v"
				BDB_LIB_NAME="db-$BDB_VERSION_MAJOR.$BDB_VERSION_MINOR"
				CPPFLAGS="$OLDCPPFLAGS -I$BDB_INCLUDE_DIR"
				LDFLAGS="$OLDLDFLAGS -L$BDB_LIB_DIR"
				AC_CHECK_LIB($BDB_LIB_NAME, db_env_create,
					[
						HCACHE_LIBS="$HCACHE_LIBS -l$BDB_LIB_NAME"
						AC_DEFINE(HAVE_BDB, 1, [Berkeley DB Support])
						hcache_db_used="bdb $hcache_db_used"
						build_hc_bdb="yes"
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
			HCACHE_LIBS="$HCACHE_LIBS -llmdb"
			hcache_db_used="lmdb $hcache_db_used"
			build_hc_lmdb="yes"
		],[
			AC_MSG_ERROR(Unable to find LMDB)
		]),	AC_MSG_ERROR(Unable to find LMDB))
fi

AM_CONDITIONAL(BUILD_HCACHE, test -n "$hcache_db_used")
if test -n "$hcache_db_used"; then
	AC_DEFINE(USE_HCACHE, 1, [Enable header caching])
	HCACHE_LIBS="-Lhcache -lhcache $HCACHE_LIBS"
	HCACHE_DEPS="hcache/libhcache.a"
else
	# For outputting in the summary
	hcache_db_used="no"
fi

AM_CONDITIONAL(BUILD_HC_BDB,  test "x$build_hc_bdb"  = "xyes")
AM_CONDITIONAL(BUILD_HC_GDBM, test "x$build_hc_gdbm" = "xyes")
AM_CONDITIONAL(BUILD_HC_KC,   test "x$build_hc_kc"   = "xyes")
AM_CONDITIONAL(BUILD_HC_LMDB, test "x$build_hc_lmdb" = "xyes")
AM_CONDITIONAL(BUILD_HC_QDBM, test "x$build_hc_qdbm" = "xyes")
AM_CONDITIONAL(BUILD_HC_TC,   test "x$build_hc_tc"   = "xyes")
dnl -- end cache --

MUTT_MD5="mutt_md5$EXEEXT"

AC_SUBST(MUTT_MD5)
AC_SUBST(MUTT_LIB_OBJECTS)
AC_SUBST(HCACHE_LIBS)
AC_SUBST(HCACHE_DEPS)

dnl -- iconv/gettext --

AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])

AM_ICONV
if test "$am_cv_func_iconv" != yes; then
    AC_MSG_ERROR([An iconv implementation is required])
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

    AC_SEARCH_LIBS([stringprep_check_version], [idn], [
        AC_DEFINE([HAVE_LIBIDN], 1, [Define to 1 if you have the GNU idn library])

        LIBS="$LIBS $LIBICONV"
        AC_CHECK_FUNCS(idna_to_unicode_utf8_from_utf8 idna_to_unicode_8z8z)
        AC_CHECK_FUNCS(idna_to_ascii_from_utf8 idna_to_ascii_8z)
        AC_CHECK_FUNCS(idna_to_ascii_lz idna_to_ascii_from_locale)
    ])

    if test "$with_idn" != auto; then
        if test $have_stringprep_h = no || test $have_idna_h = no || test $ac_cv_search_stringprep_check_version = no; then
            AC_MSG_ERROR([IDN was requested, but libidn was not usable on this system])
        fi
	fi
fi

dnl -- locales --

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
#include <wchar.h>
#include <wctype.h>]], [[mbrtowc(0, 0, 0, 0); wctomb(0, 0); wcwidth(0);
	iswprint(0); iswspace(0); towlower(0); towupper(0); iswalnum(0)]])],[mutt_cv_wc_funcs=yes],[]))
	wc_funcs=$mutt_cv_wc_funcs
fi

if test $wc_funcs = yes; then
	AC_DEFINE(HAVE_WC_FUNCS,1,[ Define if you are using the system's wchar_t functions. ])
else
	MUTT_LIB_OBJECTS="$MUTT_LIB_OBJECTS utf8.o wcwidth.o"
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
	m4/Makefile po/Makefile.in hcache/Makefile ncrypt/Makefile)
AC_OUTPUT

AC_MSG_NOTICE([Summary of build options:

  Version:           ${PACKAGE_VERSION}
  Host OS:           ${host_os}
  Install prefix:    ${prefix}
  Compiler:          ${CC}
  CFlags:            ${CFLAGS} ${CPPFLAGS}
  LDFlags:           ${LDFLAGS}
  Libs:              ${LIBS}
  Header cache libs: ${HCACHE_LIBS}

  GPGME:             $use_gpgme
  PGP:               $use_pgp
  SMIME:             $use_smime
  Notmuch:           $use_notmuch
  Header Cache(s):   $hcache_db_used
  Lua:               $use_lua
])
