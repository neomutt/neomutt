static const char rcsid[]="$Id$";
/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#define MAIN_C 1

#include "mutt.h"
#include "mutt_curses.h"
#include "keymap.h"
#include "mailbox.h"
#include "reldate.h"

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

const char ReachingUs[] = N_("\
To contact the developers, please mail to <mutt-dev@mutt.org>.\n");

const char Notice[] = N_("\
Copyright (C) 1996-8 Michael R. Elkins and others.\n\
Mutt comes with ABSOLUTELY NO WARRANTY; for details type `mutt -vv'.\n\
Mutt is free software, and you are welcome to redistribute it\n\
under certain conditions; type `mutt -vv' for details.\n");

const char Copyright[] = N_("\
Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>\n\
Copyright (C) 1997-8 Thomas Roessler <roessler@guug.de>\n\
Copyright (C) 1998   Werner Koch <wk@isil.d.shuttle.de>\n\
Copyright (C) 1998   Ruslan Ermilov <ru@ucb.crimea.ua>\n\
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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n\
");
#ifdef _PGPPATH

const char ShaCopyright[] = N_("\n\
SHA1 implementation Copyright (C) 1995-7 Eric A. Young <eay@cryptsoft.com>\n\
\n\
    Redistribution and use in source and binary forms, with or without\n\
    modification, are permitted under certain conditions.\n\
\n\
    The SHA1 implementation comes AS IS, and ANY EXPRESS OR IMPLIED\n\
    WARRANTIES, including, but not limited to, the implied warranties of\n\
    merchantability and fitness for a particular purpose ARE DISCLAIMED.\n\
\n\
    You should have received a copy of the full distribution terms\n\
    along with this program; if not, write to the program's developers.\n\
");
#endif


void mutt_exit (int code)
{
  mutt_endwin (NULL);
  exit (code);
}

static void mutt_usage (void)
{
  printf ("Mutt %s (%s)\n", MUTT_VERSION, ReleaseDate);
  puts _(
"usage: mutt [ -nRzZ ] [ -e <cmd> ] [ -F <file> ] [ -m <type> ] [ -f <file> ]\n\
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

  printf ("Mutt %s (%s)\n", MUTT_VERSION, ReleaseDate);
  puts (_(Notice));

  uname (&uts);

#ifdef _AIX
  printf ("System: %s %s.%s", uts.sysname, uts.version, uts.release);
#elif defined (SCO)
  printf ("System: SCO %s", uts.release);
#else
  printf ("System: %s %s", uts.sysname, uts.release);
#endif

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
#ifdef USE_IMAP
        "+USE_IMAP  "
#else
        "-USE_IMAP  "
#endif

#ifdef USE_POP
	"+USE_POP  "
#else
	"-USE_POP  "
#endif

#ifdef HAVE_REGCOMP
	"+HAVE_REGCOMP  "
#else
	"-HAVE_REGCOMP  "
#endif

#ifdef USE_GNU_RX
	"+USE_GNU_RX  "
#else
	"-USE_GNU_RX  "
#endif

#ifdef HAVE_COLOR
	"+HAVE_COLOR  "
#else
	"-HAVE_COLOR  "
#endif



#ifdef _PGPPATH
#ifdef HAVE_PGP5
	"+HAVE_PGP5  "
#endif
#ifdef HAVE_PGP2
	"+HAVE_PGP2  "
#endif
#ifdef HAVE_GPG
	"+HAVE_GPG  "
#endif
#endif



#ifdef BUFFY_SIZE
	"+BUFFY_SIZE "
#else
	"-BUFFY_SIZE "
#endif
	);
  puts (
#ifdef EXACT_ADDRESS
	"+"
#else
	"-"
#endif
	"EXACT_ADDRESS  "
#ifdef ENABLE_NLS
	"+"
#else
	"-"
#endif
	"ENABLE_NLS"
	);

  printf ("SENDMAIL=\"%s\"\n", SENDMAIL);
  printf ("MAILPATH=\"%s\"\n", MAILPATH);
  printf ("SHAREDIR=\"%s\"\n", SHAREDIR);
  printf ("SYSCONFDIR=\"%s\"\n", SYSCONFDIR);

#ifdef ISPELL
  printf ("ISPELL=\"%s\"\n", ISPELL);
#else
  puts ("-ISPELL");
#endif



#ifdef _PGPPATH
  printf ("_PGPPATH=\"%s\"\n", _PGPPATH);
# ifdef _PGPV2PATH
  printf ("_PGPV2PATH=\"%s\"\n", _PGPV2PATH);
# endif
# ifdef _PGPV3PATH
  printf ("_PGPV3PATH=\"%s\"\n", _PGPV3PATH);
# endif
# ifdef _PGPGPPATH
  printf ("_PGPGPGPATH=\"%s\"\n", _PGPGPGPATH);
# endif
#endif


  puts(_(ReachingUs));

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
#ifdef USE_SLANG_CURSES
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

  mutt_error = mutt_nocurses_error;
  SRAND (time (NULL));
  setlocale (LC_CTYPE, "");
  umask (077);

  memset (Options, 0, sizeof (Options));

  while ((i = getopt (argc, argv, "a:b:F:f:c:d:e:H:s:i:hm:npRvxyzZ")) != EOF)
    switch (i)
    {
      case 'a':
	attach = mutt_add_list (attach, optarg);
	break;

      case 'F':
	FREE (&Muttrc);
	Muttrc = safe_strdup (optarg);
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
      printf ("Mutt %s (%s)\n", MUTT_VERSION, ReleaseDate);
      puts (_(Copyright));
#ifdef _PGPPATH
      puts(_(ShaCopyright));
#endif
      puts (_(ReachingUs));
      exit (0);
  }

  /* Check for a batch send. */
  if (!isatty (0))
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

  if (newMagic)
    mx_set_magic (newMagic);

  if (!option (OPTNOCURSES))
  {
    SETCOLOR (MT_COLOR_NORMAL);
    clear ();
    mutt_error = mutt_curses_error;
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
	msg->env->to = rfc822_parse_adrlist (msg->env->to, argv[i]);

      if (!msg->env->to && !msg->env->cc)
      {
	if (!option (OPTNOCURSES))
	  mutt_endwin (NULL);
	fputs (_("No recipients specified.\n"), stderr);
	exit (1);
      }

      msg->env->subject = safe_strdup (subject);
      
      if (includeFile)
	infile = includeFile;
    }

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
      mutt_mktemp (buf);
      tempfile = safe_strdup (buf);

      if (draftFile)
	msg->env = mutt_read_rfc822_header (fin, NULL, 1);

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

	mutt_copy_stream (fin, fout);
	fclose (fout);
	if (fin != stdin)
	  fclose (fin);
      }
    }

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
      mutt_buffy (folder);
    }
    else if (flags & M_SELECT)
    {
      folder[0] = 0;
      mutt_select_file (folder, sizeof (folder), 1);
      if (!folder[0])
      {
	mutt_endwin (NULL);
	exit (0);
      }
    }

    if (!folder[0])
      strfcpy (folder, NONULL(Spoolfile), sizeof (folder));
    mutt_expand_path (folder, sizeof (folder));

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
      mutt_endwin (NULL);
    }
    else
      mutt_endwin (Errorbuf);
  }

  exit (0);
}
