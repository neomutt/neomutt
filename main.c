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

/**
 * @page main Command line processing
 *
 * Command line processing
 */

#define MAIN_C 1
#define GNULIB_defined_setlocale

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
#include "config/lib.h"
#include "email/lib.h"
#include "conn/conn.h"
#include "mutt.h"
#include "account.h"
#include "alias.h"
#include "browser.h"
#include "color.h"
#include "context.h"
#include "curs_lib.h"
#include "curs_main.h"
#include "globals.h"
#include "hook.h"
#include "keymap.h"
#include "mailbox.h"
#include "menu.h"
#include "mutt_curses.h"
#include "mutt_history.h"
#include "mutt_logging.h"
#include "mutt_window.h"
#include "muttlib.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "options.h"
#include "protos.h"
#include "send.h"
#include "sendlib.h"
#include "terminal.h"
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
#include "nntp/nntp.h"
#endif

/* These Config Variables are only used in main.c */
bool ResumeEditedDraftFiles; ///< Config: Resume editing previously saved draft files

#define MUTT_IGNORE (1 << 0)  /* -z */
#define MUTT_MAILBOX (1 << 1) /* -Z */
#define MUTT_NOSYSRC (1 << 2) /* -n */
#define MUTT_RO (1 << 3)      /* -R */
#define MUTT_SELECT (1 << 4)  /* -y */
#ifdef USE_NNTP
#define MUTT_NEWS (1 << 5) /* -g and -G */
#endif

/**
 * test_parse_set - Test the config parsing
 */
static void test_parse_set(void)
{
  const char *vars[] = {
    "from",        // ADDRESS
    "beep",        // BOOL
    "ispell",      // COMMAND
    "mbox_type",   // MAGIC
    "to_chars",    // MBTABLE
    "net_inc",     // NUMBER
    "signature",   // PATH
    "print",       // QUAD
    "mask",        // REGEX
    "sort",        // SORT
    "attribution", // STRING
    "zzz",         // UNKNOWN
    "my_var",      // MY_VAR
  };

  const char *commands[] = {
    "set",
    "toggle",
    "reset",
    "unset",
  };

  const char *tests[] = {
    "%s %s",       "%s %s=42",  "%s %s?",     "%s ?%s",    "%s ?%s=42",
    "%s ?%s?",     "%s no%s",   "%s no%s=42", "%s no%s?",  "%s inv%s",
    "%s inv%s=42", "%s inv%s?", "%s &%s",     "%s &%s=42", "%s &%s?",
  };

  struct Buffer *tmp = mutt_buffer_alloc(STRING);
  struct Buffer *err = mutt_buffer_alloc(STRING);
  char line[64];

  for (size_t v = 0; v < mutt_array_size(vars); v++)
  {
    // printf("--------------------------------------------------------------------------------\n");
    // printf("VARIABLE %s\n", vars[v]);
    for (size_t c = 0; c < mutt_array_size(commands); c++)
    {
      // printf("----------------------------------------\n");
      // printf("COMMAND %s\n", commands[c]);
      for (size_t t = 0; t < mutt_array_size(tests); t++)
      {
        mutt_buffer_reset(tmp);
        mutt_buffer_reset(err);

        snprintf(line, sizeof(line), tests[t], commands[c], vars[v]);
        printf("%-26s", line);
        int rc = mutt_parse_rc_line(line, tmp, err);
        printf("%2d %s\n", rc, err->data);
      }
      printf("\n");
    }
    // printf("\n");
  }

  mutt_buffer_free(&tmp);
  mutt_buffer_free(&err);
}

/**
 * reset_tilde - Temporary measure
 */
static void reset_tilde(struct ConfigSet *cs)
{
  static const char *names[] = {
    "alias_file", "certificate_file", "debug_file",
    "folder",     "history_file",     "mbox",
    "newsrc",     "news_cache_dir",   "postponed",
    "record",     "signature",
  };

  struct Buffer *value = mutt_buffer_alloc(STRING);
  for (size_t i = 0; i < mutt_array_size(names); i++)
  {
    struct HashElem *he = cs_get_elem(cs, names[i]);
    if (!he)
      continue;
    mutt_buffer_reset(value);
    cs_he_initial_get(cs, he, value);
    mutt_expand_path(value->data, value->dsize);
    cs_he_initial_set(cs, he, value->data, NULL);
    cs_he_reset(cs, he, NULL);
  }
  mutt_buffer_free(&value);
}

/**
 * mutt_exit - Leave NeoMutt NOW
 * @param code Value to return to the calling environment
 */
void mutt_exit(int code)
{
  mutt_endwin();
  exit(code);
}

// clang-format off
/**
 * usage - Display NeoMutt command line
 */
static void usage(void)
{
  puts(mutt_make_version());

  /* L10N: Try to limit to 80 columns */
  puts(_("usage:\n"
         "  neomutt [-Enx] [-e <command>] [-F <config>] [-H <draft>] [-i <include>]\n"
         "          [-b <address>] [-c <address>] [-s <subject>] [-a <file> [...] --]\n"
         "          <address> [...]\n"
         "  neomutt [-nx] [-e <command>] [-F <config>] [-b <address>] [-c <address>]\n"
         "          [-s <subject>] [-a <file> [...] --] <address> [...] < message\n"
         "  neomutt [-nRy] [-e <command>] [-F <config>] [-f <mailbox>] [-m <type>]\n"
         "  neomutt [-n] [-e <command>] [-F <config>] -A <alias>\n"
         "  neomutt [-n] [-e <command>] [-F <config>] -B\n"
         "  neomutt [-n] [-e <command>] [-F <config>] -D [-S]\n"
         "  neomutt [-n] [-e <command>] [-F <config>] -d <level> -l <file>\n"
         "  neomutt [-n] [-e <command>] [-F <config>] -G\n"
         "  neomutt [-n] [-e <command>] [-F <config>] -g <server>\n"
         "  neomutt [-n] [-e <command>] [-F <config>] -p\n"
         "  neomutt [-n] [-e <command>] [-F <config>] -Q <variable>\n"
         "  neomutt [-n] [-e <command>] [-F <config>] -Z\n"
         "  neomutt [-n] [-e <command>] [-F <config>] -z [-f <mailbox>]\n"
         "  neomutt -v[v]\n"));

  /* L10N: Try to limit to 80 columns
           If more space is needed add an indented line */
  puts(_("options:\n"
         "  --            Special argument forces NeoMutt to stop option parsing and treat\n"
         "                remaining arguments as addresses even if they start with a dash\n"
         "  -A <alias>    Print an expanded version of the given alias to stdout and exit\n"
         "  -a <file>     Attach one or more files to a message (must be the last option)\n"
         "                Add any addresses after the '--' argument\n"
         "  -B            Run in batch mode (do not start the ncurses UI)\n"
         "  -b <address>  Specify a blind carbon copy (Bcc) recipient\n"
         "  -c <address>  Specify a carbon copy (Cc) recipient\n"
         "  -D            Dump all config variables as 'name=value' pairs to stdout\n"
         "  -D -S         Like -D, but hide the value of sensitive variables\n"
         "  -d <level>    Log debugging output to a file (default is \"~/.neomuttdebug0\")\n"
         "                The level can range from 1-5 and affects verbosity\n"
         "  -E            Edit draft (-H) or include (-i) file during message composition\n"
         "  -e <command>  Specify a command to be run after reading the config files\n"
         "  -F <config>   Specify an alternative initialization file to read\n"
         "  -f <mailbox>  Specify a mailbox (as defined with 'mailboxes' command) to load\n"
         "  -G            Start NeoMutt with a listing of subscribed newsgroups\n"
         "  -g <server>   Like -G, but start at specified news server\n"
         "  -H <draft>    Specify a draft file with header and body for message composing\n"
         "  -h            Print this help message and exit\n"
         "  -i <include>  Specify an include file to be embedded in the body of a message\n"
         "  -l <file>     Specify a file for debugging output (default \"~/.neomuttdebug0\")\n"
         "  -m <type>     Specify a default mailbox format type for newly created folders\n"
         "                The type is either MH, MMDF, Maildir or mbox (case-insensitive)\n"
         "  -n            Do not read the system-wide configuration file\n"
         "  -p            Resume a prior postponed message, if any\n"
         "  -Q <variable> Query a configuration variable and print its value to stdout\n"
         "                (after the config has been read and any commands executed)\n"
         "  -R            Open mailbox in read-only mode\n"
         "  -s <subject>  Specify a subject (must be enclosed in quotes if it has spaces)\n"
         "  -v            Print the NeoMutt version and compile-time definitions and exit\n"
         "  -vv           Print the NeoMutt license and copyright information and exit\n"
         "  -x            Simulate the mailx(1) send mode\n"
         "  -y            Start NeoMutt with a listing of all defined mailboxes\n"
         "  -Z            Open the first mailbox with new message or exit immediately with\n"
         "                exit code 1 if none is found in all defined mailboxes\n"
         "  -z            Open the first or specified (-f) mailbox if it holds any message\n"
         "                or exit immediately with exit code 1 otherwise"));
}
// clang-format on

/**
 * start_curses - Start the curses or slang UI
 * @retval 0 Success
 * @retval 1 Failure
 */
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
    mutt_error(_("Error initializing terminal"));
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
  /* Now that curses is set up, we drop back to normal screen mode.
   * This simplifies displaying error messages to the user.
   * The first call to refresh() will swap us back to curses screen mode. */
  endwin();
  return 0;
}

/**
 * init_locale - Initialise the Locale/NLS settings
 */
static void init_locale(void)
{
  setlocale(LC_ALL, "");

#ifdef ENABLE_NLS
  const char *domdir = mutt_str_getenv("TEXTDOMAINDIR");
  if (domdir)
    bindtextdomain(PACKAGE, domdir);
  else
    bindtextdomain(PACKAGE, MUTTLOCALEDIR);
  textdomain(PACKAGE);
#endif
#ifndef LOCALES_HACK
  const char *p = NULL;
  /* Do we have a locale definition? */
  if ((p = mutt_str_getenv("LC_ALL")) || (p = mutt_str_getenv("LANG")) ||
      (p = mutt_str_getenv("LC_CTYPE")))
  {
    OptLocales = true;
  }
#endif
}

/**
 * get_user_info - Find the user's name, home and shell
 * @param cs Config Set
 * @retval true Success
 *
 * Find the login name, real name, home directory and shell.
 */
bool get_user_info(struct ConfigSet *cs)
{
  mutt_str_replace(&Username, mutt_str_getenv("USER"));
  mutt_str_replace(&HomeDir, mutt_str_getenv("HOME"));

  const char *shell = mutt_str_getenv("SHELL");
  if (shell)
    cs_str_initial_set(cs, "shell", shell, NULL);

  /* Get some information about the user */
  struct passwd *pw = getpwuid(getuid());
  if (pw)
  {
    if (!Username)
      Username = mutt_str_strdup(pw->pw_name);
    if (!HomeDir)
      HomeDir = mutt_str_strdup(pw->pw_dir);
    if (!shell)
      cs_str_initial_set(cs, "shell", pw->pw_shell, NULL);
  }

  if (!Username)
  {
    mutt_error(_("unable to determine username"));
    return false; // TEST05: neomutt (unset $USER, delete user from /etc/passwd)
  }

  if (!HomeDir)
  {
    mutt_error(_("unable to determine home directory"));
    return false; // TEST06: neomutt (unset $HOME, delete user from /etc/passwd)
  }

  cs_str_reset(cs, "shell", NULL);
  return true;
}

/**
 * main - Start NeoMutt
 * @param argc Number of command line arguments
 * @param argv List of command line arguments
 * @param envp Copy of the environment
 * @retval 0 Success
 * @retval 1 Error
 */
int main(int argc, char *argv[], char *envp[])
{
  char folder[PATH_MAX] = "";
  char *subject = NULL;
  char *include_file = NULL;
  char *draft_file = NULL;
  char *new_magic = NULL;
  char *dlevel = NULL;
  char *dfile = NULL;
#ifdef USE_NNTP
  char *cli_nntp = NULL;
#endif
  struct Email *msg = NULL;
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
  bool test_config = false;
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

  init_locale();

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
    i = getopt(argc, argv, "+A:a:Bb:F:f:c:Dd:l:Ee:g:GH:i:hm:npQ:RSs:TvxyzZ");
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
          sendflags |= SEND_POSTPONED;
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
        case 'T':
          test_config = true;
          break;
        case 'v':
          version++;
          break;
        case 'x': /* mailx compatible send mode */
          sendflags |= SEND_MAILX;
          break;
        case 'y': /* My special hack mode */
          flags |= MUTT_SELECT;
          break;
        case 'Z':
          flags |= MUTT_MAILBOX | MUTT_IGNORE;
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

  Config = init_config(500);
  if (!Config)
    goto main_curses;

  if (!get_user_info(Config))
    goto main_exit;

  if (test_config)
  {
    cs_str_initial_set(Config, "from", "rich@flatcap.org", NULL);
    cs_str_reset(Config, "from", NULL);
    myvar_set("my_var", "foo");
    test_parse_set();
    goto main_ok;
  }

  reset_tilde(Config);

  if (dfile)
  {
    cs_str_initial_set(Config, "debug_file", dfile, NULL);
    cs_str_reset(Config, "debug_file", NULL);
  }

  if (dlevel)
  {
    short num = 0;
    if ((mutt_str_atos(dlevel, &num) < 0) || (num < LL_MESSAGE) || (num > LL_DEBUG5))
    {
      mutt_error(_("Error: value '%s' is invalid for -d"), dlevel);
      goto main_exit; // TEST07: neomutt -d xyz
    }
    cs_str_initial_set(Config, "debug_level", dlevel, NULL);
    cs_str_reset(Config, "debug_level", NULL);
  }

  mutt_log_prep();
  if (dlevel)
    mutt_log_start();

  MuttLogger = log_disp_queue;

  if (!STAILQ_EMPTY(&cc_list) || !STAILQ_EMPTY(&bcc_list))
  {
    msg = mutt_email_new();
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
    sendflags = SEND_BATCH;
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

    if (crc != 0)
      goto main_curses; // TEST08: can't test -- fake term?

    /* check whether terminal status is supported (must follow curses init) */
    TsSupported = mutt_ts_capability();
    mutt_window_reflow();
  }

  /* set defaults and read init files */
  if (mutt_init(flags & MUTT_NOSYSRC, &commands) != 0)
    goto main_curses;

  /* The command line overrides the config */
  if (dlevel)
    cs_str_reset(Config, "debug_level", NULL);
  if (dfile)
    cs_str_reset(Config, "debug_file", NULL);

  if (mutt_log_start() < 0)
  {
    mutt_perror("log file");
    goto main_exit;
  }

  mutt_list_free(&commands);

#ifdef USE_NNTP
  /* "$news_server" precedence: command line, config file, environment, system file */
  if (cli_nntp)
    cs_str_string_set(Config, "news_server", cli_nntp, NULL);
  if (!NewsServer)
  {
    const char *env_nntp = mutt_str_getenv("NNTPSERVER");
    cs_str_string_set(Config, "news_server", env_nntp, NULL);
  }
  if (!NewsServer)
  {
    char buffer[1024];
    char *server =
        mutt_file_read_keyword(SYSCONFDIR "/nntpserver", buffer, sizeof(buffer));
    cs_str_string_set(Config, "news_server", server, NULL);
  }
  if (NewsServer)
    cs_str_initial_set(Config, "news_server", NewsServer, NULL);
#endif

  /* Initialize crypto backends.  */
  crypt_init();

  if (new_magic)
  {
    struct Buffer *err = mutt_buffer_new();
    int r = cs_str_initial_set(Config, "mbox_type", new_magic, err);
    if (CSR_RESULT(r) != CSR_SUCCESS)
    {
      mutt_error(err->data);
      mutt_buffer_free(&err);
      goto main_curses;
    }
    mutt_buffer_free(&err);
    cs_str_reset(Config, "mbox_type", NULL);
  }

  if (!STAILQ_EMPTY(&queries))
  {
    rc = mutt_query_variables(&queries);
    goto main_curses;
  }

  if (dump_variables)
  {
    dump_config(Config, CS_DUMP_STYLE_NEO, hide_sensitive ? CS_DUMP_HIDE_SENSITIVE : 0);
    goto main_ok; // TEST18: neomutt -D
  }

  if (!STAILQ_EMPTY(&alias_queries))
  {
    rc = 0;
    struct Address *a = NULL;
    for (; optind < argc; optind++)
      mutt_list_insert_tail(&alias_queries, mutt_str_strdup(argv[optind]));
    struct ListNode *np = NULL;
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
    char fpath[PATH_MAX];

    mutt_str_strfcpy(fpath, Folder, sizeof(fpath));
    mutt_expand_path(fpath, sizeof(fpath));
    bool skip = false;
#ifdef USE_IMAP
    /* we're not connected yet - skip mail folder creation */
    skip |= (imap_path_probe(fpath, NULL) == MUTT_IMAP);
#endif
#ifdef USE_NNTP
    skip |= (nntp_path_probe(fpath, NULL) == MUTT_NNTP);
#endif
    if (!skip && (stat(fpath, &sb) == -1) && (errno == ENOENT))
    {
      char msg2[STRING];
      snprintf(msg2, sizeof(msg2), _("%s does not exist. Create it?"), Folder);
      if (mutt_yesorno(msg2, MUTT_YES) == MUTT_YES)
      {
        if ((mkdir(fpath, 0700) == -1) && (errno != EEXIST))
          mutt_error(_("Can't create %s: %s"), Folder, strerror(errno)); // TEST21: neomutt -n -F /dev/null (and ~/Mail doesn't exist)
      }
    }
  }

  if (batch_mode)
  {
    goto main_ok; // TEST22: neomutt -B
  }

  cs_add_listener(Config, mutt_hist_listener);
  cs_add_listener(Config, mutt_log_listener);
  cs_add_listener(Config, mutt_menu_listener);
  cs_add_listener(Config, mutt_reply_listener);

  if (sendflags & SEND_POSTPONED)
  {
    if (!OptNoCurses)
      mutt_flushinp();
    if (ci_send_message(SEND_POSTPONED, NULL, NULL, NULL, NULL) == 0)
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
    char expanded_infile[PATH_MAX];

    if (!OptNoCurses)
      mutt_flushinp();

    if (!msg)
      msg = mutt_email_new();
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
      mutt_error(_("No recipients specified"));
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
        sendflags |= SEND_NO_FREE_HEADER;

      /* Parse the draft_file into the full Header/Body structure.
       * Set SEND_DRAFT_FILE so ci_send_message doesn't overwrite
       * our msg->content.
       */
      if (draft_file)
      {
        struct Envelope *opts_env = msg->env;
        struct stat st;

        sendflags |= SEND_DRAFT_FILE;

        /* Set up a "context" header with just enough information so that
         * mutt_prepare_template() can parse the message in fin.
         */
        struct Email *context_hdr = mutt_email_new();
        context_hdr->offset = 0;
        context_hdr->content = mutt_body_new();
        if (fstat(fileno(fin), &st) != 0)
        {
          mutt_perror(draft_file);
          goto main_curses; // TEST31: can't test
        }
        context_hdr->content->length = st.st_size;

        if (mutt_prepare_template(fin, NULL, msg, context_hdr, false) < 0)
        {
          mutt_error(_("Cannot parse message template: %s"), draft_file);
          mutt_env_free(&opts_env);
          mutt_email_free(&context_hdr);
          goto main_curses;
        }

        /* Scan for neomutt header to set ResumeDraftFiles */
        struct ListNode *np, *tmp;
        STAILQ_FOREACH_SAFE(np, &msg->env->userhdrs, entries, tmp)
        {
          if (mutt_str_strncasecmp("X-Mutt-Resume-Draft:", np->data, 20) == 0)
          {
            if (ResumeEditedDraftFiles)
              cs_str_native_set(Config, "resume_draft_files", true, NULL);

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
        mutt_email_free(&context_hdr);
      }
      /* Editing the include_file: pass it directly in.
       * Note that SEND_NO_FREE_HEADER is set above so it isn't unlinked.
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

      struct ListNode *np = NULL;
      STAILQ_FOREACH(np, &attach, entries)
      {
        if (a)
        {
          a->next = mutt_make_file_attach(np->data);
          a = a->next;
        }
        else
        {
          a = mutt_make_file_attach(np->data);
          msg->content = a;
        }
        if (!a)
        {
          mutt_error(_("%s: unable to attach file"), np->data);
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
          mutt_encode_descriptions(msg->content, true);
          mutt_prepare_envelope(msg->env, false);
          mutt_env_to_intl(msg->env, NULL, NULL);
        }

        mutt_rfc822_write_header(fout, msg->env, msg->content, -1, false);
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

      mutt_email_free(&msg);
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
    if (flags & MUTT_MAILBOX)
    {
#ifdef USE_IMAP
      bool passive = ImapPassive;
      ImapPassive = false;
#endif
      if (mutt_mailbox_check(0) == 0)
      {
        mutt_message(_("No mailbox with new mail"));
        goto main_curses; // TEST37: neomutt -Z (no new mail)
      }
      folder[0] = '\0';
      mutt_mailbox(folder, sizeof(folder));
#ifdef USE_IMAP
      ImapPassive = passive;
#endif
    }
    else if (flags & MUTT_SELECT)
    {
#ifdef USE_NNTP
      if (flags & MUTT_NEWS)
      {
        OptNews = true;
        CurrentNewsSrv = nntp_select_server(Context->mailbox, NewsServer, false);
        if (!CurrentNewsSrv)
          goto main_curses; // TEST38: neomutt -G (unset news_server)
      }
      else
#endif
          if (STAILQ_EMPTY(&AllMailboxes))
      {
        mutt_error(_("No incoming mailboxes defined"));
        goto main_curses; // TEST39: neomutt -n -F /dev/null -y
      }
      folder[0] = '\0';
      mutt_select_file(folder, sizeof(folder),
                       MUTT_SEL_FOLDER | MUTT_SEL_MAILBOX, NULL, NULL);
      if (folder[0] == '\0')
      {
        goto main_ok; // TEST40: neomutt -y (quit selection)
      }
    }

    if (!folder[0])
    {
      if (Spoolfile)
        mutt_str_strfcpy(folder, Spoolfile, sizeof(folder));
      else if (Folder)
        mutt_str_strfcpy(folder, Folder, sizeof(folder));
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
          mutt_error(_("Mailbox is empty"));
          goto main_curses; // TEST42: neomutt -z -f /dev/null
      }
    }

    mutt_folder_hook(folder);
    mutt_startup_shutdown_hook(MUTT_STARTUP_HOOK);

    repeat_error = true;
    Context = mx_mbox_open(NULL, folder,
                           ((flags & MUTT_RO) || ReadOnly) ? MUTT_READONLY : 0);
    if (Context || !explicit_folder)
    {
#ifdef USE_SIDEBAR
      mutt_sb_set_open_mailbox();
#endif
      mutt_index_menu();
      if (Context)
        mutt_context_free(&Context);
    }
#ifdef USE_IMAP
    imap_logout_all();
#endif
#ifdef USE_SASL
    mutt_sasl_done();
#endif
    log_queue_empty();
    mutt_log_stop();
    cs_free(&Config);
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
  mutt_list_free(&queries);
  crypto_module_free();
  mutt_window_free();
  mutt_buffer_pool_free();
  mutt_envlist_free();
  mutt_free_opts();
  mutt_free_keys();
  cs_free(&Config);
  return rc;
}
