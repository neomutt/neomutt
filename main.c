/**
 * @file
 * Command line processing
 *
 * @authors
 * Copyright (C) 1996-2007,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2007 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2004 g10 Code GmbH
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define MAIN_C 1

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "mutt.h"
#include "alias.h"
#include "body.h"
#include "buffy.h"
#include "envelope.h"
#include "globals.h"
#include "header.h"
#include "keymap.h"
#include "mailbox.h"
#include "mutt_curses.h"
#include "mutt_logging.h"
#include "mutt_menu.h"
#include "mutt_window.h"
#include "myvar.h"
#include "ncrypt/ncrypt.h"
#include "options.h"
#include "protos.h"
#include "terminal.h"
#include "url.h"
#include "version.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#ifdef USE_SIDEBAR
#include "sidebar.h"
#endif
#ifdef USE_IMAP
#include "imap/imap.h"
#endif
#ifdef USE_NNTP
#include "nntp.h"
#endif

void mutt_exit(int code)
{
  mutt_endwin();
  exit(code);
}

// clang-format off
static void usage(void)
{
  puts(mutt_make_version());

  puts(_("usage: neomutt [<options>] [-z] [-f <file> | -yZ]\n"
         "       neomutt [<options>] [-Ex] [-Hi <file>] [-s <subj>] [-bc <addr>] [-a <file> [...] --] <addr> [...]\n"
         "       neomutt [<options>] [-x] [-s <subj>] [-bc <addr>] [-a <file> [...] --] <addr> [...] < message\n"
         "       neomutt [<options>] -p\n"
         "       neomutt [<options>] -A <alias> [...]\n"
         "       neomutt [<options>] -Q <query> [...]\n"
         "       neomutt [<options>] -B\n"
         "       neomutt [<options>] -D [-S]\n"
         "       neomutt -v[v]\n"));

  puts(_("options:\n"
         "  -A <alias>    expand the given alias\n"
         "  -a <file> [...] --    attach file(s) to the message\n"
         "                the list of files must be terminated with the \"--\" sequence\n"
         "  -b <address>  specify a blind carbon-copy (BCC) address\n"
         "  -c <address>  specify a carbon-copy (CC) address\n"
         "  -D            print the value of all variables to stdout\n"
         "  -D -S         like -D, but hide the value of sensitive variables\n"
         "  -B            run in batch mode (do not start the ncurses UI)"));
  puts(_("  -d <level>    log debugging output to ~/.neomuttdebug0"));
  puts(_(
         "  -E            edit the draft (-H) or include (-i) file\n"
         "  -e <command>  specify a command to be executed after initialization\n"
         "  -f <file>     specify which mailbox to read\n"
         "  -F <file>     specify an alternate neomuttrc file\n"
         "  -g <server>   specify a news server (if compiled with NNTP)\n"
         "  -G            select a newsgroup (if compiled with NNTP)\n"
         "  -H <file>     specify a draft file to read header and body from\n"
         "  -i <file>     specify a file which NeoMutt should include in the body\n"
         "  -m <type>     specify a default mailbox type\n"
         "  -n            causes NeoMutt not to read the system neomuttrc\n"
         "  -p            recall a postponed message"));

  puts(_("  -Q <variable> query a configuration variable\n"
         "  -R            open mailbox in read-only mode\n"
         "  -s <subj>     specify a subject (must be in quotes if it has spaces)\n"
         "  -v            show version and compile-time definitions\n"
         "  -x            simulate the mailx send mode\n"
         "  -y            select a mailbox specified in your 'mailboxes' list\n"
         "  -z            exit immediately if there are no messages in the mailbox\n"
         "  -Z            open the first folder with new message, exit immediately if none\n"
         "  -h            this help message"));
}
// clang-format on

static int start_curses(void)
{
  km_init(); /* must come before mutt_init */

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
  mutt_signal_init();
#endif
  if (!initscr())
  {
    mutt_error(_("Error initializing terminal."));
    return 1;
  }
  /* slang requires the signal handlers to be set after initializing */
  mutt_signal_init();
  ci_start_color();
  keypad(stdscr, true);
  cbreak();
  noecho();
  nonl();
#ifdef HAVE_TYPEAHEAD
  typeahead(-1); /* simulate smooth scrolling */
#endif
#ifdef HAVE_META
  meta(stdscr, true);
#endif
  init_extended_keys();
  mutt_window_reflow();
  return 0;
}

#define MUTT_IGNORE (1 << 0)  /* -z */
#define MUTT_BUFFY (1 << 1)   /* -Z */
#define MUTT_NOSYSRC (1 << 2) /* -n */
#define MUTT_RO (1 << 3)      /* -R */
#define MUTT_SELECT (1 << 4)  /* -y */
#ifdef USE_NNTP
#define MUTT_NEWS (1 << 5) /* -g and -G */
#endif

/**
 * get_user_info - Find the user's name, home and shell
 * @param cs Config Set
 * @retval 0 Success
 * @retval 1 Error
 */
static int get_user_info(void)
{
  const char *p = mutt_str_getenv("HOME");
  if (p)
    HomeDir = mutt_str_strdup(p);

  /* Get some information about the user */
  struct passwd *pw = getpwuid(getuid());
  if (pw)
  {
    char rnbuf[STRING];

    Username = mutt_str_strdup(pw->pw_name);
    if (!HomeDir)
      HomeDir = mutt_str_strdup(pw->pw_dir);

    RealName = mutt_str_strdup(mutt_gecos_name(rnbuf, sizeof(rnbuf), pw));
    Shell = mutt_str_strdup(pw->pw_shell);
    endpwent();
  }

  if (!Username)
  {
    p = mutt_str_getenv("USER");
    if (p)
      Username = mutt_str_strdup(p);
  }

  if (!Username)
  {
    mutt_error(_("unable to determine username"));
    return 1; // TEST05: neomutt (unset $USER, delete user from /etc/passwd)
  }

  if (!HomeDir)
  {
    mutt_error(_("unable to determine home directory"));
    return 1; // TEST06: neomutt (unset $HOME, delete user from /etc/passwd)
  }

  if (!Shell)
  {
    p = mutt_str_getenv("SHELL");
    if (!p)
      p = "/bin/sh";
    Shell = mutt_str_strdup(p);
  }

  return 0;
}

/**
 * main - Start NeoMutt
 * @param argc Number of command line arguments
 * @param argv List of command line arguments
 * @param envp Copy of the environment
 * @retval 0 on success
 * @retval 1 on error
 */
int main(int argc, char *argv[], char *envp[])
{
  char folder[_POSIX_PATH_MAX] = "";
  char *subject = NULL;
  char *include_file = NULL;
  char *draft_file = NULL;
  char *new_magic = NULL;
  char *dlevel = NULL;
  char *dfile = NULL;
#ifdef USE_NNTP
  char *cli_nntp = NULL;
#endif
  struct Header *msg = NULL;
  struct ListHead attach = STAILQ_HEAD_INITIALIZER(attach);
  struct ListHead commands = STAILQ_HEAD_INITIALIZER(commands);
  struct ListHead queries = STAILQ_HEAD_INITIALIZER(queries);
  struct ListHead alias_queries = STAILQ_HEAD_INITIALIZER(alias_queries);
  struct ListHead cc_list = STAILQ_HEAD_INITIALIZER(cc_list);
  struct ListHead bcc_list = STAILQ_HEAD_INITIALIZER(bcc_list);
  int sendflags = 0;
  int flags = 0;
  int version = 0;
  int i;
  bool explicit_folder = false;
  bool dump_variables = false;
  bool hide_sensitive = false;
  bool batch_mode = false;
  bool edit_infile = false;
  extern char *optarg;
  extern int optind;
  int double_dash = argc, nargc = 1;
  int rc = 1;
  bool repeat_error = false;

  MuttLogger = log_disp_terminal;

  /* sanity check against stupid administrators */
  if (getegid() != getgid())
  {
    mutt_error("%s: I don't want to run with privileges!", argv[0]);
    goto main_exit; // TEST01: neomutt (as root, chgrp mail neomutt; chmod +s neomutt)
  }

  setlocale(LC_ALL, "");

#ifdef ENABLE_NLS
  /* FIXME what about the LOCALES_HACK in mutt_init() [init.c] ? */
  {
    const char *domdir = mutt_str_getenv("TEXTDOMAINDIR");
    if (domdir)
      bindtextdomain(PACKAGE, domdir);
    else
      bindtextdomain(PACKAGE, MUTTLOCALEDIR);
    textdomain(PACKAGE);
  }
#endif

  int out = 0;
  if (mutt_randbuf(&out, sizeof(out)) < 0)
    goto main_exit; // TEST02: neomutt (as root on non-Linux OS, rename /dev/urandom)

  umask(077);

  mutt_envlist_init(envp);

  for (optind = 1; optind < double_dash;)
  {
    /* We're getopt'ing POSIXLY, so we'll be here every time getopt()
     * encounters a non-option.  That could be a file to attach
     * (all non-options between -a and --) or it could be an address
     * (which gets collapsed to the front of argv).
     */
    for (; optind < argc; optind++)
    {
      if ((argv[optind][0] == '-') && (argv[optind][1] != '\0'))
      {
        if ((argv[optind][1] == '-') && (argv[optind][2] == '\0'))
          double_dash = optind; /* quit outer loop after getopt */
        break;                  /* drop through to getopt */
      }

      /* non-option, either an attachment or address */
      if (!STAILQ_EMPTY(&attach))
        mutt_list_insert_tail(&attach, mutt_str_strdup(argv[optind]));
      else
        argv[nargc++] = argv[optind];
    }

    /* USE_NNTP 'g:G' */
    i = getopt(argc, argv, "+A:a:Bb:F:f:c:Dd:l:Ee:g:GH:s:i:hm:npQ:RSvxyzZ");
    if (i != EOF)
    {
      switch (i)
      {
        case 'A':
          mutt_list_insert_tail(&alias_queries, mutt_str_strdup(optarg));
          break;
        case 'a':
          mutt_list_insert_tail(&attach, mutt_str_strdup(optarg));
          break;
        case 'B':
          batch_mode = true;
          break;
        case 'b':
          mutt_list_insert_tail(&bcc_list, mutt_str_strdup(optarg));
          break;
        case 'c':
          mutt_list_insert_tail(&cc_list, mutt_str_strdup(optarg));
          break;
        case 'D':
          dump_variables = true;
          break;
        case 'd':
          dlevel = optarg;
          break;
        case 'E':
          edit_infile = true;
          break;
        case 'e':
          mutt_list_insert_tail(&commands, mutt_str_strdup(optarg));
          break;
        case 'F':
          mutt_list_insert_tail(&Muttrc, mutt_str_strdup(optarg));
          break;
        case 'f':
          mutt_str_strfcpy(folder, optarg, sizeof(folder));
          explicit_folder = true;
          break;
#ifdef USE_NNTP
        case 'g': /* Specify a news server */
          cli_nntp = optarg;
          /* fallthrough */
        case 'G': /* List of newsgroups */
          flags |= MUTT_SELECT | MUTT_NEWS;
          break;
#endif
        case 'H':
          draft_file = optarg;
          break;
        case 'i':
          include_file = optarg;
          break;
        case 'l':
          dfile = optarg;
          break;
        case 'm':
          new_magic = optarg;
          break;
        case 'n':
          flags |= MUTT_NOSYSRC;
          break;
        case 'p':
          sendflags |= SENDPOSTPONED;
          break;
        case 'Q':
          mutt_list_insert_tail(&queries, mutt_str_strdup(optarg));
          break;
        case 'R':
          flags |= MUTT_RO; /* read-only mode */
          break;
        case 'S':
          hide_sensitive = true;
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
        case 'Z':
          flags |= MUTT_BUFFY | MUTT_IGNORE;
          break;
        case 'z':
          flags |= MUTT_IGNORE;
          break;
        default:
          usage();
          OptNoCurses = true;
          goto main_ok; // TEST03: neomutt -9
      }
    }
  }

  /* collapse remaining argv */
  while (optind < argc)
    argv[nargc++] = argv[optind++];
  optind = 1;
  argc = nargc;

  if (version > 0)
  {
    log_queue_flush(log_disp_terminal);
    if (version == 1)
      print_version();
    else
      print_copyright();
    OptNoCurses = true;
    goto main_ok; // TEST04: neomutt -v
  }

  if (get_user_info() != 0)
  {
    goto main_exit;
  }

  if (dfile)
  {
    set_default_value("debug_file", (intptr_t) mutt_str_strdup(dfile));
    mutt_str_replace(&DebugFile, dfile);
  }
  else
  {
    /* Make sure that the DebugFile has a value */
    LogAllowDebugSet = true;
    reset_value("debug_file");
    LogAllowDebugSet = false;
  }

  if (dlevel)
  {
    short num = 0;
    if ((mutt_str_atos(dlevel, &num) < 0) || (num < LL_MESSAGE) || (num > LL_DEBUG5))
    {
      mutt_error(_("Error: value '%s' is invalid for -d."), dlevel);
      goto main_exit; // TEST07: neomutt -d xyz
    }
    set_default_value("debug_level", (intptr_t) num);
    DebugLevel = num;
  }

  if (dlevel)
    mutt_log_start();
  else
    LogAllowDebugSet = true;

  MuttLogger = log_disp_queue;

  if (!STAILQ_EMPTY(&cc_list) || !STAILQ_EMPTY(&bcc_list))
  {
    msg = mutt_header_new();
    msg->env = mutt_env_new();

    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &bcc_list, entries)
    {
      msg->env->bcc = mutt_addr_parse_list(msg->env->bcc, np->data);
    }

    STAILQ_FOREACH(np, &cc_list, entries)
    {
      msg->env->cc = mutt_addr_parse_list(msg->env->cc, np->data);
    }

    mutt_list_free(&bcc_list);
    mutt_list_free(&cc_list);
  }

  /* Check for a batch send. */
  if (!isatty(0) || !STAILQ_EMPTY(&queries) || !STAILQ_EMPTY(&alias_queries) ||
      dump_variables || batch_mode)
  {
    OptNoCurses = true;
    sendflags = SENDBATCH;
    MuttLogger = log_disp_terminal;
    log_queue_flush(log_disp_terminal);
  }

  /* Always create the mutt_windows because batch mode has some shared code
   * paths that end up referencing them. */
  mutt_window_init();

  /* This must come before mutt_init() because curses needs to be started
   * before calling the init_pair() function to set the color scheme.  */
  if (!OptNoCurses)
  {
    int crc = start_curses();
    /* Now that curses is set up, we drop back to normal screen mode.
     * This simplifies displaying error messages to the user.
     * The first call to refresh() will swap us back to curses screen mode. */
    endwin();

    if (crc != 0)
      goto main_curses; // TEST08: can't test -- fake term?

    /* check whether terminal status is supported (must follow curses init) */
    TsSupported = mutt_ts_capability();
  }

  /* set defaults and read init files */
  if (mutt_init(flags & MUTT_NOSYSRC, &commands) != 0)
    goto main_curses;

  /* The command line overrides the config */
  if (dlevel)
    reset_value("debug_level");
  if (dfile)
    reset_value("debug_file");

  if (mutt_log_start() < 0)
  {
    mutt_perror("log file");
    goto main_exit;
  }

  LogAllowDebugSet = true;

  mutt_list_free(&commands);

#ifdef USE_NNTP
  /* "$news_server" precedence: command line, environment, config file, system file */
  const char *env_nntp = NULL;
  if (cli_nntp)
    mutt_str_replace(&NewsServer, cli_nntp);
  else if ((env_nntp = mutt_str_getenv("NNTPSERVER")))
    mutt_str_replace(&NewsServer, env_nntp);
  else if (!NewsServer)
  {
    char buffer[1024];
    char *server = mutt_file_read_keyword(SYSCONFDIR "/nntpserver", buffer, sizeof(buffer));
    NewsServer = mutt_str_strdup(server);
  }
  if (NewsServer)
    set_default_value("news_server", (intptr_t) mutt_str_strdup(NewsServer));
#endif

  /* Initialize crypto backends.  */
  crypt_init();

  if (new_magic)
  {
    mx_set_magic(new_magic);
    set_default_value("mbox_type", (intptr_t) MboxType);
  }

  if (!STAILQ_EMPTY(&queries))
  {
    for (; optind < argc; optind++)
      mutt_list_insert_tail(&queries, mutt_str_strdup(argv[optind]));
    rc = mutt_query_variables(&queries);
    mutt_list_free(&queries);
    goto main_curses;
  }

  if (dump_variables)
  {
    rc = mutt_dump_variables(hide_sensitive);
    goto main_curses; // TEST18: neomutt -D
  }

  if (!STAILQ_EMPTY(&alias_queries))
  {
    rc = 0;
    struct Address *a = NULL;
    for (; optind < argc; optind++)
      mutt_list_insert_tail(&alias_queries, mutt_str_strdup(argv[optind]));
    struct ListNode *np;
    STAILQ_FOREACH(np, &alias_queries, entries)
    {
      a = mutt_alias_lookup(np->data);
      if (a)
      {
        /* output in machine-readable form */
        mutt_addrlist_to_intl(a, NULL);
        mutt_write_address_list(a, stdout, 0, 0);
      }
      else
      {
        rc = 1;
        printf("%s\n", np->data); // TEST19: neomutt -A unknown
      }
    }
    mutt_list_free(&alias_queries);
    goto main_curses; // TEST20: neomutt -A alias
  }

  if (!OptNoCurses)
  {
    NORMAL_COLOR;
    clear();
    MuttLogger = log_disp_curses;
    log_queue_flush(log_disp_curses);
    log_queue_set_max_size(100);
  }

  /* Create the Folder directory if it doesn't exist. */
  if (!OptNoCurses && Folder)
  {
    struct stat sb;
    char fpath[_POSIX_PATH_MAX];

    mutt_str_strfcpy(fpath, Folder, sizeof(fpath));
    mutt_expand_path(fpath, sizeof(fpath));
    bool skip = false;
#ifdef USE_IMAP
    /* we're not connected yet - skip mail folder creation */
    skip |= mx_is_imap(fpath);
#endif
#ifdef USE_NNTP
    skip |= mx_is_nntp(fpath);
#endif
    if (!skip && (stat(fpath, &sb) == -1) && (errno == ENOENT))
    {
      char msg2[STRING];
      snprintf(msg2, sizeof(msg2), _("%s does not exist. Create it?"), Folder);
      if (mutt_yesorno(msg2, MUTT_YES) == MUTT_YES)
      {
        if ((mkdir(fpath, 0700) == -1) && (errno != EEXIST))
          mutt_error(_("Can't create %s: %s."), Folder, strerror(errno)); // TEST21: neomutt -n -F /dev/null (and ~/Mail doesn't exist)
      }
    }
  }

  if (batch_mode)
  {
    goto main_ok; // TEST22: neomutt -B
  }

  if (sendflags & SENDPOSTPONED)
  {
    if (!OptNoCurses)
      mutt_flushinp();
    if (ci_send_message(SENDPOSTPONED, NULL, NULL, NULL, NULL) == 0)
      rc = 0;
    // TEST23: neomutt -p (postponed message, cancel)
    // TEST24: neomutt -p (no postponed message)
    log_queue_empty();
    repeat_error = true;
    goto main_curses;
  }
  else if (subject || msg || sendflags || draft_file || include_file ||
           !STAILQ_EMPTY(&attach) || optind < argc)
  {
    FILE *fin = NULL;
    FILE *fout = NULL;
    char *tempfile = NULL, *infile = NULL;
    char *bodytext = NULL, *bodyfile = NULL;
    int rv = 0;
    char expanded_infile[_POSIX_PATH_MAX];

    if (!OptNoCurses)
      mutt_flushinp();

    if (!msg)
      msg = mutt_header_new();
    if (!msg->env)
      msg->env = mutt_env_new();

    for (i = optind; i < argc; i++)
    {
      if (url_check_scheme(argv[i]) == U_MAILTO)
      {
        if (url_parse_mailto(msg->env, &bodytext, argv[i]) < 0)
        {
          mutt_error(_("Failed to parse mailto: link"));
          goto main_curses; // TEST25: neomutt mailto:
        }
      }
      else
        msg->env->to = mutt_addr_parse_list(msg->env->to, argv[i]);
    }

    if (!draft_file && Autoedit && !msg->env->to && !msg->env->cc)
    {
      mutt_error(_("No recipients specified."));
      goto main_curses; // TEST26: neomutt -s test (with autoedit=yes)
    }

    if (subject)
      msg->env->subject = mutt_str_strdup(subject);

    if (draft_file)
    {
      infile = draft_file;
      include_file = NULL;
    }
    else if (include_file)
      infile = include_file;
    else
      edit_infile = false;

    if (infile || bodytext)
    {
      /* Prepare fin and expanded_infile. */
      if (infile)
      {
        if (mutt_str_strcmp("-", infile) == 0)
        {
          if (edit_infile)
          {
            mutt_error(_("Cannot use -E flag with stdin"));
            goto main_curses; // TEST27: neomutt -E -H -
          }
          fin = stdin;
        }
        else
        {
          mutt_str_strfcpy(expanded_infile, infile, sizeof(expanded_infile));
          mutt_expand_path(expanded_infile, sizeof(expanded_infile));
          fin = fopen(expanded_infile, "r");
          if (!fin)
          {
            mutt_perror(expanded_infile);
            goto main_curses; // TEST28: neomutt -E -H missing
          }
        }
      }

      /* Copy input to a tempfile, and re-point fin to the tempfile.
       * Note: stdin is always copied to a tempfile, ensuring draft_file
       * can stat and get the correct st_size below.
       */
      if (!edit_infile)
      {
        char buf[LONG_STRING];
        mutt_mktemp(buf, sizeof(buf));
        tempfile = mutt_str_strdup(buf);

        fout = mutt_file_fopen(tempfile, "w");
        if (!fout)
        {
          mutt_file_fclose(&fin);
          mutt_perror(tempfile);
          FREE(&tempfile);
          goto main_curses; // TEST29: neomutt -H existing-file (where tmpdir=/path/to/FILE blocking tmpdir)
        }
        if (fin)
        {
          mutt_file_copy_stream(fin, fout);
          if (fin != stdin)
            mutt_file_fclose(&fin);
        }
        else if (bodytext)
          fputs(bodytext, fout);
        mutt_file_fclose(&fout);

        fin = fopen(tempfile, "r");
        if (!fin)
        {
          mutt_perror(tempfile);
          FREE(&tempfile);
          goto main_curses; // TEST30: can't test
        }
      }
      /* If editing the infile, keep it around afterwards so
       * it doesn't get unlinked, and we can rebuild the draft_file
       */
      else
        sendflags |= SENDNOFREEHEADER;

      /* Parse the draft_file into the full Header/Body structure.
       * Set SENDDRAFTFILE so ci_send_message doesn't overwrite
       * our msg->content.
       */
      if (draft_file)
      {
        struct Envelope *opts_env = msg->env;
        struct stat st;

        sendflags |= SENDDRAFTFILE;

        /* Set up a "context" header with just enough information so that
         * mutt_prepare_template() can parse the message in fin.
         */
        struct Header *context_hdr = mutt_header_new();
        context_hdr->offset = 0;
        context_hdr->content = mutt_body_new();
        if (fstat(fileno(fin), &st) != 0)
        {
          mutt_perror(draft_file);
          goto main_curses; // TEST31: can't test
        }
        context_hdr->content->length = st.st_size;

        if (mutt_prepare_template(fin, NULL, msg, context_hdr, 0) < 0)
        {
          mutt_error(_("Cannot parse message template: %s"), draft_file);
          mutt_env_free(&opts_env);
          mutt_header_free(&context_hdr);
          goto main_curses;
        }

        /* Scan for neomutt header to set ResumeDraftFiles */
        struct ListNode *np, *tmp;
        STAILQ_FOREACH_SAFE(np, &msg->env->userhdrs, entries, tmp)
        {
          if (mutt_str_strncasecmp("X-Mutt-Resume-Draft:", np->data, 20) == 0)
          {
            if (ResumeEditedDraftFiles)
              ResumeDraftFiles = true;

            STAILQ_REMOVE(&msg->env->userhdrs, np, ListNode, entries);
            FREE(&np->data);
            FREE(&np);
          }
        }

        mutt_addr_append(&msg->env->to, opts_env->to, false);
        mutt_addr_append(&msg->env->cc, opts_env->cc, false);
        mutt_addr_append(&msg->env->bcc, opts_env->bcc, false);
        if (opts_env->subject)
          mutt_str_replace(&msg->env->subject, opts_env->subject);

        mutt_env_free(&opts_env);
        mutt_header_free(&context_hdr);
      }
      /* Editing the include_file: pass it directly in.
       * Note that SENDNOFREEHEADER is set above so it isn't unlinked.
       */
      else if (edit_infile)
        bodyfile = expanded_infile;
      /* For bodytext and unedited include_file: use the tempfile.
       */
      else
        bodyfile = tempfile;

      if (fin)
        mutt_file_fclose(&fin);
    }

    FREE(&bodytext);

    if (!STAILQ_EMPTY(&attach))
    {
      struct Body *a = msg->content;

      while (a && a->next)
        a = a->next;

      struct ListNode *np;
      STAILQ_FOREACH(np, &attach, entries)
      {
        if (a)
        {
          a->next = mutt_make_file_attach(np->data);
          a = a->next;
        }
        else
          msg->content = a = mutt_make_file_attach(np->data);
        if (!a)
        {
          mutt_error(_("%s: unable to attach file."), np->data);
          mutt_list_free(&attach);
          goto main_curses; // TEST32: neomutt john@example.com -a missing
        }
      }
      mutt_list_free(&attach);
    }

    rv = ci_send_message(sendflags, msg, bodyfile, NULL, NULL);
    /* We WANT the "Mail sent." and any possible, later error */
    log_queue_empty();
    if (ErrorBufMessage)
      mutt_message("%s", ErrorBuf);

    if (edit_infile)
    {
      if (include_file)
        msg->content->unlink = false;
      else if (draft_file)
      {
        if (truncate(expanded_infile, 0) == -1)
        {
          mutt_perror(expanded_infile);
          goto main_curses; // TEST33: neomutt -H read-only -s test john@example.com -E
        }
        fout = mutt_file_fopen(expanded_infile, "a");
        if (!fout)
        {
          mutt_perror(expanded_infile);
          goto main_curses; // TEST34: can't test
        }

        /* If the message was sent or postponed, these will already
         * have been done.
         */
        if (rv < 0)
        {
          if (msg->content->next)
            msg->content = mutt_make_multipart(msg->content);
          mutt_encode_descriptions(msg->content, 1);
          mutt_prepare_envelope(msg->env, 0);
          mutt_env_to_intl(msg->env, NULL, NULL);
        }

        mutt_rfc822_write_header(fout, msg->env, msg->content, -1, 0);
        if (ResumeEditedDraftFiles)
          fprintf(fout, "X-Mutt-Resume-Draft: 1\n");
        fputc('\n', fout);
        if ((mutt_write_mime_body(msg->content, fout) == -1))
        {
          mutt_file_fclose(&fout);
          goto main_curses; // TEST35: can't test
        }
        mutt_file_fclose(&fout);
      }

      mutt_header_free(&msg);
    }

    /* !edit_infile && draft_file will leave the tempfile around */
    if (tempfile)
    {
      unlink(tempfile);
      FREE(&tempfile);
    }

    mutt_window_free();

    if (rv != 0)
      goto main_curses; // TEST36: neomutt -H existing -s test john@example.com -E (cancel sending)
  }
  else
  {
    if (flags & MUTT_BUFFY)
    {
      if (mutt_buffy_check(false) == 0)
      {
        mutt_message(_("No mailbox with new mail."));
        goto main_curses; // TEST37: neomutt -Z (no new mail)
      }
      folder[0] = '\0';
      mutt_buffy(folder, sizeof(folder));
    }
    else if (flags & MUTT_SELECT)
    {
#ifdef USE_NNTP
      if (flags & MUTT_NEWS)
      {
        OptNews = true;
        CurrentNewsSrv = nntp_select_server(NewsServer, false);
        if (!CurrentNewsSrv)
          goto main_curses; // TEST38: neomutt -G (unset news_server)
      }
      else
#endif
          if (!Incoming)
      {
        mutt_error(_("No incoming mailboxes defined."));
        goto main_curses; // TEST39: neomutt -n -F /dev/null -y
      }
      folder[0] = '\0';
      mutt_select_file(folder, sizeof(folder), MUTT_SEL_FOLDER | MUTT_SEL_BUFFY, NULL, NULL);
      if (folder[0] == '\0')
      {
        goto main_ok; // TEST40: neomutt -y (quit selection)
      }
    }

    if (!folder[0])
    {
      if (SpoolFile)
        mutt_str_strfcpy(folder, NONULL(SpoolFile), sizeof(folder));
      else if (Folder)
        mutt_str_strfcpy(folder, NONULL(Folder), sizeof(folder));
      /* else no folder */
    }

#ifdef USE_NNTP
    if (OptNews)
    {
      OptNews = false;
      nntp_expand_path(folder, sizeof(folder), &CurrentNewsSrv->conn->account);
    }
    else
#endif
      mutt_expand_path(folder, sizeof(folder));

    mutt_str_replace(&CurrentFolder, folder);
    mutt_str_replace(&LastFolder, folder);

    if (flags & MUTT_IGNORE)
    {
      /* check to see if there are any messages in the folder */
      switch (mx_check_empty(folder))
      {
        case -1:
          mutt_perror(folder);
          goto main_curses; // TEST41: neomutt -z -f missing
        case 1:
          mutt_error(_("Mailbox is empty."));
          goto main_curses; // TEST42: neomutt -z -f /dev/null
      }
    }

    mutt_folder_hook(folder);
    mutt_startup_shutdown_hook(MUTT_STARTUPHOOK);

    repeat_error = true;
    Context = mx_open_mailbox(
        folder, ((flags & MUTT_RO) || ReadOnly) ? MUTT_READONLY : 0, NULL);
    if (Context || !explicit_folder)
    {
#ifdef USE_SIDEBAR
      mutt_sb_set_open_buffy();
#endif
      mutt_index_menu();
      if (Context)
        FREE(&Context);
    }
#ifdef USE_IMAP
    imap_logout_all();
#endif
#ifdef USE_SASL
    mutt_sasl_done();
#endif
    log_queue_empty();
    mutt_log_stop();
    mutt_window_free();
    // TEST43: neomutt (no change to mailbox)
    // TEST44: neomutt (change mailbox)
  }

main_ok:
  rc = 0;
main_curses:
  mutt_endwin();
  log_queue_flush(log_disp_terminal);
  mutt_log_stop();
  /* Repeat the last message to the user */
  if (repeat_error && ErrorBufMessage)
    puts(ErrorBuf);
main_exit:
  mutt_envlist_free();
  mutt_free_opts();
  mutt_free_keys();
  return rc;
}
