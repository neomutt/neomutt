
/* Is this the international version? */
#undef SUBVERSION

/* The "real" version string */
#undef VERSION

/* The package name */
#undef PACKAGE

/* Where to put l10n data */
#undef MUTTLOCALEDIR

/* Enable debugging info */
#define DEBUG

/* What is your domain name? */
#undef DOMAIN

/* use dotlocking to lock mailboxes? */
#undef USE_DOTLOCK

/* use an external dotlocking program? */
#undef DL_STANDALONE

/* use flock() to lock mailboxes? */
#undef USE_FLOCK

/* Use fcntl() to lock folders? */
#undef USE_FCNTL

/*
 * Define if you have problems with mutt not detecting new/old mailboxes
 * over NFS.  Some NFS implementations incorrectly cache the attributes
 * of small files.
 */
#undef NFS_ATTRIBUTE_HACK

/* Do you want support for the POP3 protocol? (--enable-pop) */
#undef USE_POP

/* Do you want support for the IMAP protocol? (--enable-imap) */
#undef USE_IMAP

/* Do you want support for IMAP GSSAPI authentication? (--with-gss) */
#undef USE_GSS

/* Do you have the Heimdal version of Kerberos V? (for gss support) */
#undef HAVE_HEIMDAL

/* Do you want support for SSL? (--enable-ssl) */
#undef USE_SSL

/* Avoid SSL routines which used patent-encumbered RC5 algorithms */
#undef NO_RC5

/* Avoid SSL routines which used patent-encumbered IDEA algorithms */
#undef NO_IDEA

/* Avoid SSL routines which used patent-encumbered RSA algorithms */
#undef NO_RSA

/*
 * Is mail spooled to the user's home directory?  If defined, MAILPATH should
 * be set to the filename of the spool mailbox relative the the home
 * directory.
 * use: configure --with-homespool=FILE
 */
#undef HOMESPOOL

/* Where new mail is spooled */
#undef MAILPATH

/* Does your system have the srand48() suite? */
#undef HAVE_SRAND48

/* Where to find sendmail on your system */
#undef SENDMAIL

/* Do you want PGP support (--enable-pgp)? */
#undef HAVE_PGP

/* Where to find ispell on your system? */
#undef ISPELL

/* Should Mutt run setgid "mail" ? */
#undef USE_SETGID

/* Does your curses library support color? */
#undef HAVE_COLOR

/* Compiling with SLang instead of curses/ncurses? */
#undef USE_SLANG_CURSES

/* program to use for shell commands */
#define EXECSHELL "/bin/sh"

/* The "buffy_size" feature */
#undef BUFFY_SIZE

/* The Sun mailtool attachments support */
#undef SUN_ATTACHMENT 

/* The result of isprint() is unreliable? */
#undef LOCALES_HACK

/* Enable exact regeneration of email addresses as parsed?  NOTE: this requires
   significant more memory when defined. */
#undef EXACT_ADDRESS

/* Does your system have the snprintf() call? */
#undef HAVE_SNPRINTF

/* Does your system have the vsnprintf() call? */
#undef HAVE_VSNPRINTF

/* Does your system have the fchdir() call? */
#undef HAVE_FCHDIR

/* Define if your locale.h file contains LC_MESSAGES.  */
#undef HAVE_LC_MESSAGES

/* Define to 1 if NLS is requested.  */
#undef ENABLE_NLS

/* Define as 1 if you have catgets and don't want to use GNU gettext.  */
#undef HAVE_CATGETS

/* Define as 1 if you have gettext and don't want to use GNU gettext.  */
#undef HAVE_GETTEXT

/* Do we have stpcpy? */
#undef HAVE_STPCPY

/* Use the included regex.c? */
#undef USE_GNU_REGEX

/* Where's mixmaster located? */
#undef MIXMASTER

/* Where are the character set definitions located? */
#undef CHARMAPS_DIR

/* Define to `int' if <signal.h> doesn't define.  */
#undef sig_atomic_t

/* define when your system has sys/time.h */
#undef HAVE_SYS_TIME_H

/* define when your system has sys/resource.h */
#undef HAVE_SYS_RESOURCE_H

/* define when your system has the setrlimit function */
#undef HAVE_SETRLIMIT

@BOTTOM@
/* Define if you have start_color, as a function or macro.  */
#undef HAVE_START_COLOR

/* Define if you have typeahead, as a function or macro.  */
#undef HAVE_TYPEAHEAD

/* Define if you have bkgdset, as a function or macro.  */
#undef HAVE_BKGDSET

/* Define if you have curs_set, as a function or macro.  */
#undef HAVE_CURS_SET

/* Define if you have meta, as a function or macro.  */
#undef HAVE_META

/* Define if you have use_default_colors, as a function or macro.  */
#undef HAVE_USE_DEFAULT_COLORS

/* Define if you have resizeterm, as a function or macro.  */
#undef HAVE_RESIZETERM

/* Some systems declare sig_atomic_t as volatile, smome others -- no.
 * This define will have value `sig_atomic_t' or `volatile sig_atomic_t'
 * accordingly. */
#undef SIG_ATOMIC_VOLATILE_T
