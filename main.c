/*
 * Copyright (C) 1996-2001 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1999-2001 Thomas Roessler <roessler@guug.de>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

#define MAIN_C 1

#include "mutt.h"
#include "mutt_curses.h"
#include "keymap.h"
#include "mailbox.h"
#include "url.h"

#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

static const char *ReachingUs = N_("\
To contact the developers, please mail to <mutt-dev@mutt.org>.\n\
To report a bug, please use the flea(1) utility.\n");

static const char *Notice = N_("\
Copyright (C) 1996-2001 Michael R. Elkins and others.\n\
Mutt comes with ABSOLUTELY NO WARRANTY; for details type `mutt -vv'.\n\
Mutt is free software, and you are welcome to redistribute it\n\
under certain conditions; type `mutt -vv' for details.\n");

static const char *Copyright = N_("\
Copyright (C) 1996-2001 Michael R. Elkins <me@cs.hmc.edu>\n\
Copyright (C) 1996-2001 Brandon Long <blong@fiction.net>\n\
Copyright (C) 1997-2001 Thomas Roessler <roessler@guug.de>\n\
Copyright (C) 1998-2001 Werner Koch <wk@isil.d.shuttle.de>\n\
Copyright (C) 1999-2001 Brendan Cully <brendan@kublai.com>\n\
Copyright (C) 1999-2001 Tommi Komulainen <Tommi.Komulainen@iki.fi>\n\
Copyright (C) 2000-2001 Edmund Grimley Evans <edmundo@rano.org>\n\
\n\
Lots of others not mentioned here contributed lots of code,\n\
fixes, and suggestions.\n\
\n\
    This program is free software; you can redistribute it and/or modify\n\
    it under the terms of the GNU General Public License as published by\n\
    the Free Software Foundation; either version 2 of the License, or\n\
    (at your option) any later version.\n\
\n\
    This program is distributed in the hope that it will be useful,\n\
    but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
    GNU General Public License for more details.\n\
\n\
    You should have received a copy of the GNU General Public License\n\
    along with this program; if not, write to the Free Software\n\
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.\n\
");

void mutt_exit (int code)
{
  mutt_endwin (NULL);
  exit (code);
}

static void mutt_usage (void)
{
  puts (mutt_make_version ());

  puts _(
"usage: mutt [ -nRyzZ ] [ -e <cmd> ] [ -F <file> ] [ -m <type> ] [ -f <file> ]\n\
       mutt [ -nR ] [ -e <cmd> ] [ -F <file> ] -Q <query> [ -Q <query> ] [...]\n\
       mutt [ -nx ] [ -e <cmd> ] [ -a <file> ] [ -F <file> ] [ -H <file> ] [ -i <file> ] [ -s <subj> ] [ -b <addr> ] [ -c <addr> ] <addr> [ ... ]\n\
       mutt [ -n ] [ -e <cmd> ] [ -F <file> ] -p\n\
       mutt -v[v]\n\
\n\
options:\n\
  -a <file>\tattach a file to the message\n\
  -b <address>\tspecify a blind carbon-copy (BCC) address\n\
  -c <address>\tspecify a carbon-copy (CC) address\n\
  -e <command>\tspecify a command to be executed after initialization\n\
  -f <file>\tspecify which mailbox to read\n\
  -F <file>\tspecify an alternate muttrc file\n\
  -H <file>\tspecify a draft file to read header from\n\
  -i <file>\tspecify a file which Mutt should include in the reply\n\
  -m <type>\tspecify a default mailbox type\n\
  -n\t\tcauses Mutt not to read the system Muttrc\n\
  -p\t\trecall a postponed message\n\
  -Q <variable>\tquery a configuration variable\n\
  -R\t\topen mailbox in read-only mode\n\
  -s <subj>\tspecify a subject (must be in quotes if it has spaces)\n\
  -v\t\tshow version and compile-time definitions\n\
  -x\t\tsimulate the mailx send mode\n\
  -y\t\tselect a mailbox specified in your `mailboxes' list\n\
  -z\t\texit immediately if there are no messages in the mailbox\n\
  -Z\t\topen the first folder with new message, exit immediately if none\n\
  -h\t\tthis help message");

  exit (0);
}

static void show_version (void)
{
  struct utsname uts;

  puts (mutt_make_version());
  puts (_(Notice));

  uname (&uts);

#ifdef _AIX
  printf ("System: %s %s.%s", uts.sysname, uts.version, uts.release);
#elif defined (SCO)
  printf ("System: SCO %s", uts.release);
#else
  printf ("System: %s %s", uts.sysname, uts.release);
#endif

  printf (" (%s)", uts.machine);

#ifdef NCURSES_VERSION
  printf (" [using ncurses %s]", NCURSES_VERSION);
#elif defined(USE_SLANG_CURSES)
  printf (" [using slang %d]", SLANG_VERSION);
#endif

  puts (_("\nCompile options:"));

#ifdef DOMAIN
  printf ("DOMAIN=\"%s\"\n", DOMAIN);
#else
  puts ("-DOMAIN");
#endif

#ifdef DEBUG
  puts ("+DEBUG");
#else
  puts ("-DEBUG");
#endif
  

  
  puts (

#ifdef HOMESPOOL
	"+HOMESPOOL  "
#else
	"-HOMESPOOL  "
#endif

#ifdef USE_SETGID
	"+USE_SETGID  "
#else
	"-USE_SETGID  "
#endif

#ifdef USE_DOTLOCK
	"+USE_DOTLOCK  "
#else
	"-USE_DOTLOCK  "
#endif

#ifdef DL_STANDALONE
	"+DL_STANDALONE  "
#else
	"-DL_STANDALONE  "
#endif

	"\n"
	
#ifdef USE_FCNTL
	"+USE_FCNTL  "
#else
	"-USE_FCNTL  "
#endif

#ifdef USE_FLOCK
	"+USE_FLOCK"
#else
	"-USE_FLOCK"
#endif
	);
  puts (
#ifdef USE_POP
	"+USE_POP  "
#else
	"-USE_POP  "
#endif

#ifdef USE_IMAP
        "+USE_IMAP  "
#else
        "-USE_IMAP  "
#endif

#ifdef USE_GSS
	"+USE_GSS  "
#else
	"-USE_GSS  "
#endif

	
#ifdef USE_SSL
	"+USE_SSL  "
#else
	"-USE_SSL  "
#endif

#ifdef USE_SASL
	"+USE_SASL  "
#else
	"-USE_SASL  "
#endif
	"\n"
	
#ifdef HAVE_REGCOMP
	"+HAVE_REGCOMP  "
#else
	"-HAVE_REGCOMP  "
#endif

#ifdef USE_GNU_REGEX
	"+USE_GNU_REGEX  "
#else
	"-USE_GNU_REGEX  "
#endif

	"\n"
	
#ifdef HAVE_COLOR
	"+HAVE_COLOR  "
#else
	"-HAVE_COLOR  "
#endif
	
#ifdef HAVE_START_COLOR
	"+HAVE_START_COLOR  "
#else
	"-HAVE_START_COLOR  "
#endif
	
#ifdef HAVE_TYPEAHEAD
	"+HAVE_TYPEAHEAD  "
#else
	"-HAVE_TYPEAHEAD  "
#endif
	
#ifdef HAVE_BKGDSET
	"+HAVE_BKGDSET  "
#else
	"-HAVE_BKGDSET  "
#endif

	"\n"
	
#ifdef HAVE_CURS_SET
	"+HAVE_CURS_SET  "
#else
	"-HAVE_CURS_SET  "
#endif
	
#ifdef HAVE_META
	"+HAVE_META  "
#else
	"-HAVE_META  "
#endif
	
#ifdef HAVE_RESIZETERM
	"+HAVE_RESIZETERM  "
#else
	"-HAVE_RESIZETERM  "
#endif
	
	
	);
  
  
  puts (
	
#ifdef HAVE_PGP
	"+HAVE_PGP  "
#else
	"-HAVE_PGP  "
#endif

#ifdef HAVE_SMIME
	"+HAVE_SMIME  "
#else
	"-HAVE_SMIME  "
#endif

#ifdef BUFFY_SIZE
	"+BUFFY_SIZE "
#else
	"-BUFFY_SIZE "
#endif
#ifdef EXACT_ADDRESS
	"+EXACT_ADDRESS  "
#else
	"-EXACT_ADDRESS  "
#endif

#ifdef SUN_ATTACHMENT
	"+SUN_ATTACHMENT  "
#else
	"-SUN_ATTACHMENT  "
#endif

	"\n"
	
#ifdef ENABLE_NLS
	"+ENABLE_NLS  "
#else
	"-ENABLE_NLS  "
#endif

#ifdef LOCALES_HACK
	"+LOCALES_HACK  "
#else
	"-LOCALES_HACK  "
#endif
	      
#ifdef HAVE_WC_FUNCS
	"+HAVE_WC_FUNCS  "
#else
	"-HAVE_WC_FUNCS  "
#endif
	
#ifdef HAVE_LANGINFO_CODESET
	"+HAVE_LANGINFO_CODESET  "
#else
	"-HAVE_LANGINFO_CODESET  "
#endif

	
#ifdef HAVE_LANGINFO_YESEXPR
 	"+HAVE_LANGINFO_YESEXPR  "
#else
 	"-HAVE_LANGINFO_YESEXPR  "
#endif
	
	"\n"

#if HAVE_ICONV
	"+HAVE_ICONV  "
#else
	"-HAVE_ICONV  "
#endif

#if ICONV_NONTRANS
	"+ICONV_NONTRANS  "
#else
	"-ICONV_NONTRANS  "
#endif

#if HAVE_GETSID
	"+HAVE_GETSID  "
#else
	"-HAVE_GETSID  "
#endif

#if HAVE_GETADDRINFO
	"+HAVE_GETADDRINFO  "
#else
	"-HAVE_GETADDRINFO  "
#endif

	);

#ifdef ISPELL
  printf ("ISPELL=\"%s\"\n", ISPELL);
#else
  puts ("-ISPELL");
#endif

  printf ("SENDMAIL=\"%s\"\n", SENDMAIL);
  printf ("MAILPATH=\"%s\"\n", MAILPATH);
  printf ("PKGDATADIR=\"%s\"\n", PKGDATADIR);
  printf ("SYSCONFDIR=\"%s\"\n", SYSCONFDIR);
  printf ("EXECSHELL=\"%s\"\n", EXECSHELL);
#ifdef MIXMASTER
  printf ("MIXMASTER=\"%s\"\n", MIXMASTER);
#else
  puts ("-MIXMASTER");
#endif

  puts(_(ReachingUs));

  mutt_print_patchlist();
  
  exit (0);
}

static void start_curses (void)
{
  km_init (); /* must come before mutt_init */

#ifdef USE_SLANG_CURSES
  SLtt_Ignore_Beep = 1; /* don't do that #*$@^! annoying visual beep! */
  SLsmg_Display_Eight_Bit = 128; /* characters above this are printable */
#else
  /* should come before initscr() so that ncurses 4.2 doesn't try to install
     its own SIGWINCH handler */
  mutt_signal_init ();
#endif
  if (initscr () == NULL)
  {
    puts _("Error initializing terminal.");
    exit (1);
  }
#if 1 /* USE_SLANG_CURSES  - commenting out suggested in #455. */
  /* slang requires the signal handlers to be set after initializing */
  mutt_signal_init ();
#endif
  ci_start_color ();
  keypad (stdscr, TRUE);
  cbreak ();
  noecho ();
#if HAVE_TYPEAHEAD
  typeahead (-1);       /* simulate smooth scrolling */
#endif
#if HAVE_META
  meta (stdscr, TRUE);
#endif
}

#define M_IGNORE  (1<<0)	/* -z */
#define M_BUFFY   (1<<1)	/* -Z */
#define M_NOSYSRC (1<<2)	/* -n */
#define M_RO      (1<<3)	/* -R */
#define M_SELECT  (1<<4)	/* -y */

int main (int argc, char **argv)
{
  char folder[_POSIX_PATH_MAX] = "";
  char *subject = NULL;
  char *includeFile = NULL;
  char *draftFile = NULL;
  char *newMagic = NULL;
  HEADER *msg = NULL;
  LIST *attach = NULL;
  LIST *commands = NULL;
  LIST *queries = NULL;
  int sendflags = 0;
  int flags = 0;
  int version = 0;
  int i;
  int explicit_folder = 0;
  extern char *optarg;
  extern int optind;

  /* sanity check against stupid administrators */
  
  if(getegid() != getgid())
  {
    fprintf(stderr, "%s: I don't want to run with privileges!\n",
	    argv[0]);
    exit(1);
  }

#ifdef ENABLE_NLS
  /* FIXME what about init.c:1439 ? */
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, MUTTLOCALEDIR);
  textdomain (PACKAGE);
#endif

  setlocale (LC_CTYPE, "");

  mutt_error = mutt_nocurses_error;
  mutt_message = mutt_nocurses_error;
  SRAND (time (NULL));
  umask (077);

  memset (Options, 0, sizeof (Options));
  memset (QuadOptions, 0, sizeof (QuadOptions));
  
  while ((i = getopt (argc, argv, "a:b:F:f:c:d:e:H:s:i:hm:npQ:RvxyzZ")) != EOF)
    switch (i)
    {
      case 'a':
	attach = mutt_add_list (attach, optarg);
	break;

      case 'F':
	mutt_str_replace (&Muttrc, optarg);
	break;

      case 'f':
	strfcpy (folder, optarg, sizeof (folder));
        explicit_folder = 1;
	break;

      case 'b':
      case 'c':
	if (!msg)
	  msg = mutt_new_header ();
	if (!msg->env)
	  msg->env = mutt_new_envelope ();
	if (i == 'b')
	  msg->env->bcc = rfc822_parse_adrlist (msg->env->bcc, optarg);
	else
	  msg->env->cc = rfc822_parse_adrlist (msg->env->cc, optarg);
	break;

      case 'd':
#ifdef DEBUG
	debuglevel = atoi (optarg);
	printf (_("Debugging at level %d.\n"), debuglevel);
#else
	printf _("DEBUG was not defined during compilation.  Ignored.\n");
#endif
	break;

      case 'e':
	commands = mutt_add_list (commands, optarg);
	break;

      case 'H':
	draftFile = optarg;
	break;

      case 'i':
	includeFile = optarg;
	break;

      case 'm':
	/* should take precedence over .muttrc setting, so save it for later */
	newMagic = optarg; 
	break;

      case 'n':
	flags |= M_NOSYSRC;
	break;

      case 'p':
	sendflags |= SENDPOSTPONED;
	break;

      case 'Q':
        queries = mutt_add_list (queries, optarg);
        break;
      
      case 'R':
	flags |= M_RO; /* read-only mode */
	break;

      case 's':
	subject = optarg;
	break;

      case 'v':
	version++;
	break;

      case 'x': /* mailx compatible send mode */
	sendflags |= SENDMAILX;
	break;

      case 'y': /* My special hack mode */
	flags |= M_SELECT;
	break;

      case 'z':
	flags |= M_IGNORE;
	break;

      case 'Z':
	flags |= M_BUFFY | M_IGNORE;
	break;

      default:
	mutt_usage ();
    }

  switch (version)
  {
    case 0:
      break;
    case 1:
      show_version ();
      break;
    default:
      puts (mutt_make_version ());
      puts (_(Copyright));
      puts (_(ReachingUs));
      exit (0);
  }

  /* Check for a batch send. */
  if (!isatty (0) || queries)
  {
    set_option (OPTNOCURSES);
    sendflags = SENDBATCH;
  }

  /* This must come before mutt_init() because curses needs to be started
     before calling the init_pair() function to set the color scheme.  */
  if (!option (OPTNOCURSES))
    start_curses ();

  /* set defaults and read init files */
  mutt_init (flags & M_NOSYSRC, commands);
  mutt_free_list (&commands);

  if (queries)
    return mutt_query_variables (queries);
  
  if (newMagic)
    mx_set_magic (newMagic);

  if (!option (OPTNOCURSES))
  {
    SETCOLOR (MT_COLOR_NORMAL);
    clear ();
    mutt_error = mutt_curses_error;
    mutt_message = mutt_curses_message;
  }

  /* Create the Maildir directory if it doesn't exist. */
  if (!option (OPTNOCURSES) && Maildir)
  {
    struct stat sb;
    char fpath[_POSIX_PATH_MAX];
    char msg[STRING];

    strfcpy (fpath, Maildir, sizeof (fpath));
    mutt_expand_path (fpath, sizeof (fpath));
#ifdef USE_IMAP
    /* we're not connected yet - skip mail folder creation */
    if (!mx_is_imap (fpath))
#endif
    if (stat (fpath, &sb) == -1 && errno == ENOENT)
    {
      snprintf (msg, sizeof (msg), _("%s does not exist. Create it?"), Maildir);
      if (mutt_yesorno (msg, 1) == 1)
      {
	if (mkdir (fpath, 0700) == -1 && errno != EEXIST)
	  mutt_error ( _("Can't create %s: %s."), Maildir, strerror (errno));
      }
    }
  }

  if (sendflags & SENDPOSTPONED)
  {
    if (!option (OPTNOCURSES))
      mutt_flushinp ();
    ci_send_message (SENDPOSTPONED, NULL, NULL, NULL, NULL);
    mutt_endwin (NULL);
  }
  else if (subject || msg || sendflags || draftFile || includeFile || attach ||
	   optind < argc)
  {
    FILE *fin = NULL;
    char buf[LONG_STRING];
    char *tempfile = NULL, *infile = NULL;
    char *bodytext = NULL;
    
    if (!option (OPTNOCURSES))
      mutt_flushinp ();

    if (!msg)
      msg = mutt_new_header ();

    if (draftFile)
      infile = draftFile;
    else
    {
      if (!msg->env)
	msg->env = mutt_new_envelope ();

      for (i = optind; i < argc; i++)
      {
	if (url_check_scheme (argv[i]) == U_MAILTO)
	  url_parse_mailto (msg->env, &bodytext, argv[i]);
	else
	  msg->env->to = rfc822_parse_adrlist (msg->env->to, argv[i]);
      }

      if (option (OPTAUTOEDIT) && !msg->env->to && !msg->env->cc)
      {
	if (!option (OPTNOCURSES))
	  mutt_endwin (NULL);
	fputs (_("No recipients specified.\n"), stderr);
	exit (1);
      }

      if (subject)
	msg->env->subject = safe_strdup (subject);

      if (includeFile)
	infile = includeFile;
    }

    if (infile || bodytext)
    {
      if (infile)
      {
	if (mutt_strcmp ("-", infile) == 0)
	  fin = stdin;
	else 
	{
	  char path[_POSIX_PATH_MAX];
	  
	  strfcpy (path, infile, sizeof (path));
	  mutt_expand_path (path, sizeof (path));
	  if ((fin = fopen (path, "r")) == NULL)
	  {
	    if (!option (OPTNOCURSES))
	      mutt_endwin (NULL);
	    perror (path);
	    exit (1);
	  }
	}
      }
      else
	fin = NULL;

      mutt_mktemp (buf);
      tempfile = safe_strdup (buf);

      if (draftFile)
	msg->env = mutt_read_rfc822_header (fin, NULL, 1, 0);

      /* is the following if still needed? */
      
      if (tempfile)
      {
	FILE *fout;

	if ((fout = safe_fopen (tempfile, "w")) == NULL)
	{
	  if (!option (OPTNOCURSES))
	    mutt_endwin (NULL);
	  perror (tempfile);
	  fclose (fin);
	  FREE (&tempfile);
	  exit (1);
	}
	if (fin)
	  mutt_copy_stream (fin, fout);
	else if (bodytext)
	  fputs (bodytext, fout);
	fclose (fout);
	if (fin && fin != stdin)
	  fclose (fin);
      }
    }

    safe_free ((void **) &bodytext);
    
    if (attach)
    {
      LIST *t = attach;
      BODY *a = NULL;

      while (t)
      {
	if (a)
	{
	  a->next = mutt_make_file_attach (t->data);
	  a = a->next;
	}
	else
	  msg->content = a = mutt_make_file_attach (t->data);
	if (!a)
	{
	  if (!option (OPTNOCURSES))
	    mutt_endwin (NULL);
	  fprintf (stderr, _("%s: unable to attach file.\n"), t->data);
	  mutt_free_list (&attach);
	  exit (1);
	}
	t = t->next;
      }
      mutt_free_list (&attach);
    }

    ci_send_message (sendflags, msg, tempfile, NULL, NULL);

    if (!option (OPTNOCURSES))
      mutt_endwin (NULL);
  }
  else
  {
    if (flags & M_BUFFY)
    {
      if (!mutt_buffy_check (0))
      {
	mutt_endwin _("No mailbox with new mail.");
	exit (1);
      }
      folder[0] = 0;
      mutt_buffy (folder, sizeof (folder));
    }
    else if (flags & M_SELECT)
    {
      if (!Incoming) {
	mutt_endwin _("No incoming mailboxes defined.");
	exit (1);
      }
      folder[0] = 0;
      mutt_select_file (folder, sizeof (folder), M_SEL_FOLDER | M_SEL_BUFFY);
      if (!folder[0])
      {
	mutt_endwin (NULL);
	exit (0);
      }
    }

    if (!folder[0])
      strfcpy (folder, NONULL(Spoolfile), sizeof (folder));
    mutt_expand_path (folder, sizeof (folder));

    mutt_str_replace (&LastFolder, folder);

    if (flags & M_IGNORE)
    {
      struct stat st;

      /* check to see if there are any messages in the folder */
      if (stat (folder, &st) != 0)
      {
	mutt_endwin (strerror (errno));
	exit (1);
      }

      if (st.st_size == 0)
      {
	mutt_endwin _("Mailbox is empty.");
	exit (1);
      }
    }

    mutt_folder_hook (folder);

    if((Context = mx_open_mailbox (folder, ((flags & M_RO) || option (OPTREADONLY)) ? M_READONLY : 0, NULL))
       || !explicit_folder)
    {
      mutt_index_menu ();
      if (Context)
	safe_free ((void **)&Context);
    }
    mutt_endwin (Errorbuf);
  }

  exit (0);
}
