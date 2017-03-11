/*
 * Copyright (C) 1996-2007,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2007 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2004 g10 Code GmbH
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#define MAIN_C 1

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "keymap.h"
#include "mailbox.h"
#include "url.h"
#include "mutt_crypt.h"
#include "mutt_idna.h"
#include "version.h"
#ifdef USE_SIDEBAR
#include "sidebar.h"
#endif

#ifdef USE_SASL
#include "mutt_sasl.h"
#endif

#ifdef USE_IMAP
#include "imap/imap.h"
#endif

#ifdef USE_HCACHE
#include "hcache.h"
#endif

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

#ifdef HAVE_STRINGPREP_H
#include <stringprep.h>
#elif defined(HAVE_IDN_STRINGPREP_H)
#include <idn/stringprep.h>
#endif

#ifdef USE_NNTP
#include "nntp.h"
#endif

char **envlist;

void mutt_exit (int code)
{
  mutt_endwin (NULL);
  exit (code);
}

static void mutt_usage (void)
{
  puts (mutt_make_version ());

  puts (_(
"usage: mutt [<options>] [-z] [-f <file> | -yZ]\n\
       mutt [<options>] [-Ex] [-Hi <file>] [-s <subj>] [-bc <addr>] [-a <file> [...] --] <addr> [...]\n\
       mutt [<options>] [-x] [-s <subj>] [-bc <addr>] [-a <file> [...] --] <addr> [...] < message\n\
       mutt [<options>] -p\n\
       mutt [<options>] -A <alias> [...]\n\
       mutt [<options>] -Q <query> [...]\n\
       mutt [<options>] -B\n\
       mutt [<options>] -D [-S]\n\
       mutt -v[v]\n"));

  puts (_("\
options:\n\
  -A <alias>\texpand the given alias\n\
  -a <file> [...] --\tattach file(s) to the message\n\
\t\tthe list of files must be terminated with the \"--\" sequence\n\
  -b <address>\tspecify a blind carbon-copy (BCC) address\n\
  -c <address>\tspecify a carbon-copy (CC) address\n\
  -D\t\tprint the value of all variables to stdout\n\
  -D -S\t\tlike -D, but hide the value of sensitive variables\n\
  -B\t\trun in batch mode (do not start the ncurses UI)"));
#if DEBUG
  puts (_("  -d <level>\tlog debugging output to ~/.muttdebug0"));
#endif
  puts (_(
"  -E\t\tedit the draft (-H) or include (-i) file\n\
  -e <command>\tspecify a command to be executed after initialization\n\
  -f <file>\tspecify which mailbox to read\n\
  -F <file>\tspecify an alternate muttrc file\n\
  -g <server>\tspecify a news server (if compiled with NNTP)\n\
  -G\t\tselect a newsgroup (if compiled with NNTP)\n\
  -H <file>\tspecify a draft file to read header and body from\n\
  -i <file>\tspecify a file which Mutt should include in the body\n\
  -m <type>\tspecify a default mailbox type\n\
  -n\t\tcauses Mutt not to read the system Muttrc\n\
  -p\t\trecall a postponed message"));
  
  puts (_("\
  -Q <variable>\tquery a configuration variable\n\
  -R\t\topen mailbox in read-only mode\n\
  -s <subj>\tspecify a subject (must be in quotes if it has spaces)\n\
  -v\t\tshow version and compile-time definitions\n\
  -x\t\tsimulate the mailx send mode\n\
  -y\t\tselect a mailbox specified in your `mailboxes' list\n\
  -z\t\texit immediately if there are no messages in the mailbox\n\
  -Z\t\topen the first folder with new message, exit immediately if none\n\
  -h\t\tthis help message"));

  exit (0);
}

static void start_curses (void)
{
  km_init (); /* must come before mutt_init */

#ifdef USE_SLANG_CURSES
  SLtt_Ignore_Beep = 1; /* don't do that #*$@^! annoying visual beep! */
  SLsmg_Display_Eight_Bit = 128; /* characters above this are printable */
  SLtt_set_color(0, NULL, "default", "default");
#if SLANG_VERSION >= 20000
  SLutf8_enable(-1);
#endif
#else
  /* should come before initscr() so that ncurses 4.2 doesn't try to install
     its own SIGWINCH handler */
  mutt_signal_init ();
#endif
  if (initscr () == NULL)
  {
    puts (_("Error initializing terminal."));
    exit (1);
  }
#if 1 /* USE_SLANG_CURSES  - commenting out suggested in #455. */
  /* slang requires the signal handlers to be set after initializing */
  mutt_signal_init ();
#endif
  ci_start_color ();
  keypad (stdscr, true);
  cbreak ();
  noecho ();
  nonl ();
#if HAVE_TYPEAHEAD
  typeahead (-1);       /* simulate smooth scrolling */
#endif
#if HAVE_META
  meta (stdscr, true);
#endif
init_extended_keys();
  mutt_reflow_windows ();
}

#define MUTT_IGNORE  (1<<0)	/* -z */
#define MUTT_BUFFY   (1<<1)	/* -Z */
#define MUTT_NOSYSRC (1<<2)	/* -n */
#define MUTT_RO      (1<<3)	/* -R */
#define MUTT_SELECT  (1<<4)	/* -y */
#ifdef USE_NNTP
#define MUTT_NEWS    (1<<5)	/* -g and -G */
#endif

int main (int argc, char **argv, char **environ)
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
  LIST *alias_queries = NULL;
  int sendflags = 0;
  int flags = 0;
  int version = 0;
  int i;
  int explicit_folder = 0;
  int dump_variables = 0;
  int hide_sensitive = 0;
  int batch_mode = 0;
  int edit_infile = 0;
  extern char *optarg;
  extern int optind;
  int double_dash = argc, nargc = 1;

  /* sanity check against stupid administrators */
  
  if(getegid() != getgid())
  {
    fprintf(stderr, "%s: I don't want to run with privileges!\n",
	    argv[0]);
    exit(1);
  }

  setlocale (LC_ALL, "");

#ifdef ENABLE_NLS
  /* FIXME what about init.c:1439 ? */
  bindtextdomain (PACKAGE, MUTTLOCALEDIR);
  textdomain (PACKAGE);
#endif

  mutt_error = mutt_nocurses_error;
  mutt_message = mutt_nocurses_error;
  (void)mutt_rand32();
  umask (077);

  memset (Options, 0, sizeof (Options));
  memset (QuadOptions, 0, sizeof (QuadOptions));

  /* Init envlist */
  {
    char **srcp, **dstp;
    int count = 0;
    for (srcp = environ; srcp && *srcp; srcp++)
      count++;
    envlist = safe_calloc(count+1, sizeof(char *));
    for (srcp = environ, dstp = envlist; srcp && *srcp; srcp++, dstp++)
      *dstp = safe_strdup(*srcp);
  }

  for (optind = 1; optind < double_dash; )
  {
    /* We're getopt'ing POSIXLY, so we'll be here every time getopt()
     * encounters a non-option.  That could be a file to attach 
     * (all non-options between -a and --) or it could be an address
     * (which gets collapsed to the front of argv).
     */
    for (; optind < argc; optind++)
    {
      if (argv[optind][0] == '-' && argv[optind][1] != '\0')
      {
        if (argv[optind][1] == '-' && argv[optind][2] == '\0')
          double_dash = optind; /* quit outer loop after getopt */
        break;                  /* drop through to getopt */
      }

      /* non-option, either an attachment or address */
      if (attach)
        attach = mutt_add_list (attach, argv[optind]);
      else
        argv[nargc++] = argv[optind];
    }

#ifdef USE_NNTP
    if ((i = getopt (argc, argv, "+A:a:Bb:F:f:c:Dd:Ee:g:GH:s:i:hm:npQ:RSvxyzZ")) != EOF)
#else
    if ((i = getopt (argc, argv, "+A:a:Bb:F:f:c:Dd:Ee:H:s:i:hm:npQ:RSvxyzZ")) != EOF)
#endif
      switch (i)
      {
      case 'A':
        alias_queries = mutt_add_list (alias_queries, optarg);
        break;
      case 'a':
	attach = mutt_add_list (attach, optarg);
	break;

      case 'F':
        // mutt_str_replace (&Muttrc, optarg);
        Muttrc = mutt_add_list (Muttrc, optarg);
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

      case 'D':
	dump_variables = 1;
	break;

      case 'B':
        batch_mode = 1;
        break;

      case 'd':
#ifdef DEBUG
	if (mutt_atoi (optarg, &debuglevel) < 0 || debuglevel <= 0)
	{
	  fprintf (stderr, _("Error: value '%s' is invalid for -d.\n"), optarg);
	  return 1;
	}
	printf (_("Debugging at level %d.\n"), debuglevel);
#else
	printf (_("DEBUG was not defined during compilation.  Ignored.\n"));
#endif
	break;

      case 'E':
        edit_infile = 1;
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
	flags |= MUTT_NOSYSRC;
	break;

      case 'p':
	sendflags |= SENDPOSTPONED;
	break;

      case 'Q':
        queries = mutt_add_list (queries, optarg);
        break;
      
      case 'R':
	flags |= MUTT_RO; /* read-only mode */
	break;

      case 'S':
	hide_sensitive = 1;
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
	flags |= MUTT_SELECT;
	break;

#ifdef USE_NNTP
      case 'g': /* Specify a news server */
	{
	  char buf[LONG_STRING];

	  snprintf (buf, sizeof (buf), "set news_server=%s", optarg);
	  commands = mutt_add_list (commands, buf);
	}

      case 'G': /* List of newsgroups */
	flags |= MUTT_SELECT | MUTT_NEWS;
	break;
#endif

      case 'z':
	flags |= MUTT_IGNORE;
	break;

      case 'Z':
	flags |= MUTT_BUFFY | MUTT_IGNORE;
	break;

      default:
	mutt_usage ();
      }
  }

  /* collapse remaining argv */
  while (optind < argc)
    argv[nargc++] = argv[optind++];
  optind = 1;
  argc = nargc;

  switch (version)
  {
    case 0:
      break;
    case 1:
      print_version();
      exit (0);
    default:
      print_copyright();
      exit (0);
  }

  /* Check for a batch send. */
  if (!isatty (0) || queries || alias_queries || dump_variables || batch_mode)
  {
    set_option (OPTNOCURSES);
    sendflags = SENDBATCH;
  }

  /* Always create the mutt_windows because batch mode has some shared code
   * paths that end up referencing them. */
  mutt_init_windows ();

  /* This must come before mutt_init() because curses needs to be started
     before calling the init_pair() function to set the color scheme.  */
  if (!option (OPTNOCURSES))
  {
    start_curses ();

    /* check whether terminal status is supported (must follow curses init) */
    TSSupported = mutt_ts_capability();
  }

  /* set defaults and read init files */
  mutt_init (flags & MUTT_NOSYSRC, commands);
  mutt_free_list (&commands);

  /* Initialize crypto backends.  */
  crypt_init ();

  if (newMagic)
    mx_set_magic (newMagic);

  if (queries)
  {
    for (; optind < argc; optind++)
      queries = mutt_add_list (queries, argv[optind]);
    return mutt_query_variables (queries);
  }
  if (dump_variables)
    return mutt_dump_variables(hide_sensitive);

  if (alias_queries)
  {
    int rv = 0;
    ADDRESS *a;
    for (; optind < argc; optind++)
      alias_queries = mutt_add_list (alias_queries, argv[optind]);
    for (; alias_queries; alias_queries = alias_queries->next)
    {
      if ((a = mutt_lookup_alias (alias_queries->data)))
      {	
	/* output in machine-readable form */
	mutt_addrlist_to_intl (a, NULL);
	mutt_write_address_list (a, stdout, 0, 0);
      }
      else
      {
	rv = 1;
	printf ("%s\n", alias_queries->data);
      }
    }
    return rv;
  }

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
      if (mutt_yesorno (msg, MUTT_YES) == MUTT_YES)
      {
	if (mkdir (fpath, 0700) == -1 && errno != EEXIST)
	  mutt_error (_("Can't create %s: %s."), Maildir, strerror (errno));
      }
    }
  }

  if (batch_mode)
      exit(0);

  if (sendflags & SENDPOSTPONED)
  {
    if (!option (OPTNOCURSES))
      mutt_flushinp ();
    ci_send_message (SENDPOSTPONED, NULL, NULL, NULL, NULL);
    mutt_free_windows ();
    mutt_endwin (NULL);
  }
  else if (subject || msg || sendflags || draftFile || includeFile || attach ||
	   optind < argc)
  {
    FILE *fin = NULL;
    FILE *fout = NULL;
    char buf[LONG_STRING];
    char *tempfile = NULL, *infile = NULL;
    char *bodytext = NULL, *bodyfile = NULL;
    int rv = 0;
    char expanded_infile[_POSIX_PATH_MAX];
    
    if (!option (OPTNOCURSES))
      mutt_flushinp ();

    if (!msg)
      msg = mutt_new_header ();
    if (!msg->env)
      msg->env = mutt_new_envelope ();

    for (i = optind; i < argc; i++)
    {
      if (url_check_scheme (argv[i]) == U_MAILTO)
      {
        if (url_parse_mailto (msg->env, &bodytext, argv[i]) < 0)
        {
          if (!option (OPTNOCURSES))
            mutt_endwin (NULL);
          fputs (_("Failed to parse mailto: link\n"), stderr);
          exit (1);
        }
      }
      else
        msg->env->to = rfc822_parse_adrlist (msg->env->to, argv[i]);
    }

    if (!draftFile && option (OPTAUTOEDIT) && !msg->env->to && !msg->env->cc)
    {
      if (!option (OPTNOCURSES))
        mutt_endwin (NULL);
      fputs (_("No recipients specified.\n"), stderr);
      exit (1);
    }

    if (subject)
      msg->env->subject = safe_strdup (subject);

    if (draftFile)
    {
      infile = draftFile;
      includeFile = NULL;
    }
    else if (includeFile)
      infile = includeFile;
    else
      edit_infile = 0;

    if (infile || bodytext)
    {
      /* Prepare fin and expanded_infile. */
      if (infile)
      {
	if (mutt_strcmp ("-", infile) == 0)
        {
          if (edit_infile)
          {
            fputs (_("Cannot use -E flag with stdin\n"), stderr);
            exit (1);
          }
	  fin = stdin;
        }
	else
	{
	  strfcpy (expanded_infile, infile, sizeof (expanded_infile));
	  mutt_expand_path (expanded_infile, sizeof (expanded_infile));
	  if ((fin = fopen (expanded_infile, "r")) == NULL)
	  {
	    if (!option (OPTNOCURSES))
	      mutt_endwin (NULL);
	    perror (expanded_infile);
	    exit (1);
	  }
	}
      }

      /* Copy input to a tempfile, and re-point fin to the tempfile.
       * Note: stdin is always copied to a tempfile, ensuring draftFile
       * can stat and get the correct st_size below.
       */
      if (!edit_infile)
      {
        mutt_mktemp (buf, sizeof (buf));
        tempfile = safe_strdup (buf);

        if ((fout = safe_fopen (tempfile, "w")) == NULL)
        {
          if (!option (OPTNOCURSES))
            mutt_endwin (NULL);
          perror (tempfile);
          safe_fclose (&fin);
          FREE (&tempfile);
          exit (1);
        }
        if (fin)
        {
          mutt_copy_stream (fin, fout);
          if (fin != stdin)
            safe_fclose (&fin);
        }
        else if (bodytext)
          fputs (bodytext, fout);
        safe_fclose (&fout);

        if ((fin = fopen (tempfile, "r")) == NULL)
        {
          if (!option (OPTNOCURSES))
            mutt_endwin (NULL);
          perror (tempfile);
          FREE (&tempfile);
          exit (1);
        }
      }
      /* If editing the infile, keep it around afterwards so
       * it doesn't get unlinked, and we can rebuild the draftFile
       */
      else
        sendflags |= SENDNOFREEHEADER;

      /* Parse the draftFile into the full HEADER/BODY structure.
       * Set SENDDRAFTFILE so ci_send_message doesn't overwrite
       * our msg->content.
       */
      if (draftFile)
      {
        HEADER *context_hdr = NULL;
        ENVELOPE *opts_env = msg->env;
        struct stat st;
        LIST *uh, **last_uhp;

        sendflags |= SENDDRAFTFILE;

        /* Set up a "context" header with just enough information so that
         * mutt_prepare_template() can parse the message in fin.
         */
        context_hdr = mutt_new_header ();
        context_hdr->offset = 0;
        context_hdr->content = mutt_new_body ();
        if (fstat (fileno (fin), &st))
        {
          perror (draftFile);
          exit (1);
        }
        context_hdr->content->length = st.st_size;

        mutt_prepare_template (fin, NULL, msg, context_hdr, 0);

        /* Scan for mutt header to set OPTRESUMEDRAFTFILES */
        for (last_uhp = &msg->env->userhdrs, uh = *last_uhp;
             uh; uh = *last_uhp)
        {
          if (ascii_strncasecmp ("X-Mutt-Resume-Draft:", uh->data, 20) == 0)
          {
            if (option (OPTRESUMEEDITEDDRAFTFILES))
              set_option (OPTRESUMEDRAFTFILES);

            *last_uhp = uh->next;
            uh->next = NULL;
            mutt_free_list (&uh);
          }
          else
            last_uhp = &uh->next;
        }

        rfc822_append (&msg->env->to, opts_env->to, 0);
        rfc822_append (&msg->env->cc, opts_env->cc, 0);
        rfc822_append (&msg->env->bcc, opts_env->bcc, 0);
        if (opts_env->subject)
          mutt_str_replace (&msg->env->subject, opts_env->subject);

        mutt_free_envelope (&opts_env);
        mutt_free_header (&context_hdr);
      }
      /* Editing the includeFile: pass it directly in.
       * Note that SENDNOFREEHEADER is set above so it isn't unlinked.
       */
      else if (edit_infile)
        bodyfile = expanded_infile;
      /* For bodytext and unedited includeFile: use the tempfile.
       */
      else
        bodyfile = tempfile;

      if (fin)
        safe_fclose (&fin);
    }

    FREE (&bodytext);

    if (attach)
    {
      LIST *t = attach;
      BODY *a = msg->content;

      while (a && a->next)
        a = a->next;

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

    rv = ci_send_message (sendflags, msg, bodyfile, NULL, NULL);

    if (edit_infile)
    {
      if (includeFile)
        msg->content->unlink = 0;
      else if (draftFile)
      {
        if (truncate (expanded_infile, 0) == -1)
        {
          if (!option (OPTNOCURSES))
            mutt_endwin (NULL);
          perror (expanded_infile);
          exit (1);
        }
        if ((fout = safe_fopen (expanded_infile, "a")) == NULL)
        {
          if (!option (OPTNOCURSES))
            mutt_endwin (NULL);
          perror (expanded_infile);
          exit (1);
        }

        /* If the message was sent or postponed, these will already
         * have been done.
         */
        if (rv < 0)
        {
          if (msg->content->next)
            msg->content = mutt_make_multipart (msg->content);
          mutt_encode_descriptions (msg->content, 1);
          mutt_prepare_envelope (msg->env, 0);
          mutt_env_to_intl (msg->env, NULL, NULL);
        }

        mutt_write_rfc822_header (fout, msg->env, msg->content, -1, 0);
        if (option (OPTRESUMEEDITEDDRAFTFILES))
          fprintf (fout, "X-Mutt-Resume-Draft: 1\n");
        fputc ('\n', fout);
        if ((mutt_write_mime_body (msg->content, fout) == -1))
        {
          if (!option (OPTNOCURSES))
            mutt_endwin (NULL);
          safe_fclose (&fout);
          exit (1);
        }
        safe_fclose (&fout);
      }

      mutt_free_header (&msg);
    }

    /* !edit_infile && draftFile will leave the tempfile around */
    if (tempfile)
    {
      unlink (tempfile);
      FREE (&tempfile);
    }

    mutt_free_windows ();
    if (!option (OPTNOCURSES))
      mutt_endwin (NULL);

    if (rv)
      exit(1);
  }
  else
  {
    if (flags & MUTT_BUFFY)
    {
      if (!mutt_buffy_check (0))
      {
	mutt_endwin (_("No mailbox with new mail."));
	exit (1);
      }
      folder[0] = 0;
      mutt_buffy (folder, sizeof (folder));
    }
    else if (flags & MUTT_SELECT)
    {
#ifdef USE_NNTP
      if (flags & MUTT_NEWS)
      {
	set_option (OPTNEWS);
	if(!(CurrentNewsSrv = nntp_select_server (NewsServer, 0)))
	{
	  mutt_endwin (Errorbuf);
	  exit (1);
	}
      }
      else
#endif
      if (!Incoming) {
	mutt_endwin (_("No incoming mailboxes defined."));
	exit (1);
      }
      folder[0] = 0;
      mutt_select_file (folder, sizeof (folder), MUTT_SEL_FOLDER | MUTT_SEL_BUFFY);
      if (!folder[0])
      {
	mutt_endwin (NULL);
	exit (0);
      }
    }

    if (!folder[0])
    {
      if (Spoolfile)
        strfcpy (folder, NONULL(Spoolfile), sizeof (folder));
      else if (Maildir)
        strfcpy (folder, NONULL(Maildir), sizeof (folder));
      /* else no folder */
    }

#ifdef USE_NNTP
    if (option (OPTNEWS))
    {
      unset_option (OPTNEWS);
      nntp_expand_path (folder, sizeof (folder), &CurrentNewsSrv->conn->account);
    }
    else
#endif
    mutt_expand_path (folder, sizeof (folder));

    mutt_str_replace (&CurrentFolder, folder);
    mutt_str_replace (&LastFolder, folder);

    if (flags & MUTT_IGNORE)
    {
      /* check to see if there are any messages in the folder */
      switch (mx_check_empty (folder))
      {
	case -1:
	  mutt_endwin (strerror (errno));
	  exit (1);
	case 1:
	  mutt_endwin (_("Mailbox is empty."));
	  exit (1);
      }
    }

    mutt_folder_hook (folder);
    mutt_startup_shutdown_hook (MUTT_STARTUPHOOK);

    if((Context = mx_open_mailbox (folder, ((flags & MUTT_RO) || option (OPTREADONLY)) ? MUTT_READONLY : 0, NULL))
       || !explicit_folder)
    {
#ifdef USE_SIDEBAR
      mutt_sb_set_open_buffy ();
#endif
      mutt_index_menu ();
      if (Context)
	FREE (&Context);
    }
#ifdef USE_IMAP
    imap_logout_all ();
#endif
#ifdef USE_SASL
    mutt_sasl_done ();
#endif
    mutt_free_opts ();
    mutt_free_windows ();
    mutt_endwin (Errorbuf);
  }

  exit (0);
}
