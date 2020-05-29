/**
 * @file
 * Command line processing
 *
 * @authors
 * Copyright (C) 1996-2007,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2007 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2004 g10 Code GmbH
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "debug/lib.h"
#include "browser.h"
#include "commands.h"
#include "context.h"
#include "hook.h"
#include "index.h"
#include "init.h"
#include "keymap.h"
#include "mutt_attach.h"
#include "mutt_globals.h"
#include "mutt_history.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "mx.h"
#include "myvar.h"
#include "options.h"
#include "protos.h"
#include "version.h"
#include "ncrypt/lib.h"
#include "send/lib.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#ifdef USE_SIDEBAR
#include "sidebar/lib.h"
#endif
#ifdef USE_IMAP
#include "imap/lib.h"
#endif
#ifdef USE_NNTP
#include "nntp/lib.h"
#endif
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif

/* These Config Variables are only used in main.c */
bool C_ResumeEditedDraftFiles; ///< Config: Resume editing previously saved draft files

// clang-format off
typedef uint8_t CliFlags;         ///< Flags for command line options, e.g. #MUTT_CLI_IGNORE
#define MUTT_CLI_NO_FLAGS      0  ///< No flags are set
#define MUTT_CLI_IGNORE  (1 << 0) ///< -z Open first mailbox if it has mail
#define MUTT_CLI_MAILBOX (1 << 1) ///< -Z Open first mailbox if is has new mail
#define MUTT_CLI_NOSYSRC (1 << 2) ///< -n Do not read the system-wide config file
#define MUTT_CLI_RO      (1 << 3) ///< -R Open mailbox in read-only mode
#define MUTT_CLI_SELECT  (1 << 4) ///< -y Start with a list of all mailboxes
#ifdef USE_NNTP
#define MUTT_CLI_NEWS    (1 << 5) ///< -g/-G Start with a list of all newsgroups
#endif
// clang-format on

/**
 * reset_tilde - Temporary measure
 * @param cs Config Set
 */
static void reset_tilde(struct ConfigSet *cs)
{
  static const char *names[] = { "folder", "mbox", "postponed", "record" };

  struct Buffer value = mutt_buffer_make(256);
  for (size_t i = 0; i < mutt_array_size(names); i++)
  {
    struct HashElem *he = cs_get_elem(cs, names[i]);
    if (!he)
      continue;
    mutt_buffer_reset(&value);
    cs_he_initial_get(cs, he, &value);
    mutt_buffer_expand_path_regex(&value, false);
    cs_he_initial_set(cs, he, value.data, NULL);
    cs_he_reset(cs, he, NULL);
  }
  mutt_buffer_dealloc(&value);
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

  /* L10N: Try to limit to 80 columns.  If more space is needed add an indented line */
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
#if (SLANG_VERSION >= 20000)
  SLutf8_enable(-1);
#endif
#else
  /* should come before initscr() so that ncurses 4.2 doesn't try to install
   * its own SIGWINCH handler */
  mutt_signal_init();
#endif
  if (!initscr())
  {
    mutt_error(_("Error initializing terminal"));
    return 1;
  }
  /* slang requires the signal handlers to be set after initializing */
  mutt_signal_init();
  Colors = mutt_colors_new();
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
  /* Do we have a locale definition? */
  if (mutt_str_getenv("LC_ALL") || mutt_str_getenv("LANG") || mutt_str_getenv("LC_CTYPE"))
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
static bool get_user_info(struct ConfigSet *cs)
{
  const char *shell = mutt_str_getenv("SHELL");
  if (shell)
    cs_str_initial_set(cs, "shell", shell, NULL);

  /* Get some information about the user */
  struct passwd *pw = getpwuid(getuid());
  if (pw)
  {
    if (!Username)
      Username = mutt_str_dup(pw->pw_name);
    if (!HomeDir)
      HomeDir = mutt_str_dup(pw->pw_dir);
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
 * log_translation - Log the translation being used
 *
 * Read the header info from the translation file.
 *
 * @note Call bindtextdomain() first
 */
static void log_translation(void)
{
  const char *header = ""; // Do not merge these two lines
  header = _(header);      // otherwise the .po files will end up badly ordered
  const char *lang = strcasestr(header, "Language:");
  int len = 64;
  if (lang)
  {
    lang += 9; // skip label
    SKIPWS(lang);
    char *nl = strchr(lang, '\n');
    if (nl)
      len = (nl - lang);
  }
  else
  {
    lang = "NONE";
  }

  mutt_debug(LL_DEBUG1, "Translation: %.*s\n", len, lang);
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
  char *subject = NULL;
  char *include_file = NULL;
  char *draft_file = NULL;
  char *new_type = NULL;
  char *dlevel = NULL;
  char *dfile = NULL;
#ifdef USE_NNTP
  char *cli_nntp = NULL;
#endif
  struct Email *e = NULL;
  struct ListHead attach = STAILQ_HEAD_INITIALIZER(attach);
  struct ListHead commands = STAILQ_HEAD_INITIALIZER(commands);
  struct ListHead queries = STAILQ_HEAD_INITIALIZER(queries);
  struct ListHead alias_queries = STAILQ_HEAD_INITIALIZER(alias_queries);
  struct ListHead cc_list = STAILQ_HEAD_INITIALIZER(cc_list);
  struct ListHead bcc_list = STAILQ_HEAD_INITIALIZER(bcc_list);
  SendFlags sendflags = SEND_NO_FLAGS;
  CliFlags flags = MUTT_CLI_NO_FLAGS;
  int version = 0;
  int i;
  bool explicit_folder = false;
  bool dump_variables = false;
  bool hide_sensitive = false;
  bool batch_mode = false;
  bool edit_infile = false;
#ifdef USE_DEBUG_PARSE_TEST
  bool test_config = false;
#endif
  int double_dash = argc, nargc = 1;
  int rc = 1;
  bool repeat_error = false;
  struct Buffer folder = mutt_buffer_make(0);
  struct Buffer expanded_infile = mutt_buffer_make(0);
  struct Buffer tempfile = mutt_buffer_make(0);
  struct ConfigSet *cs = NULL;

  MuttLogger = log_disp_terminal;

  /* sanity check against stupid administrators */
  if (getegid() != getgid())
  {
    mutt_error("%s: I don't want to run with privileges!", argv[0]);
    goto main_exit; // TEST01: neomutt (as root, chgrp mail neomutt; chmod +s neomutt)
  }

  init_locale();

  umask(077);

  mutt_envlist_init(envp);
  for (optind = 1; optind < double_dash;)
  {
    /* We're getopt'ing POSIXLY, so we'll be here every time getopt()
     * encounters a non-option.  That could be a file to attach
     * (all non-options between -a and --) or it could be an address
     * (which gets collapsed to the front of argv).  */
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
        mutt_list_insert_tail(&attach, mutt_str_dup(argv[optind]));
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
          mutt_list_insert_tail(&alias_queries, mutt_str_dup(optarg));
          break;
        case 'a':
          mutt_list_insert_tail(&attach, mutt_str_dup(optarg));
          break;
        case 'B':
          batch_mode = true;
          break;
        case 'b':
          mutt_list_insert_tail(&bcc_list, mutt_str_dup(optarg));
          break;
        case 'c':
          mutt_list_insert_tail(&cc_list, mutt_str_dup(optarg));
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
          mutt_list_insert_tail(&commands, mutt_str_dup(optarg));
          break;
        case 'F':
          mutt_list_insert_tail(&Muttrc, mutt_str_dup(optarg));
          break;
        case 'f':
          mutt_buffer_strcpy(&folder, optarg);
          explicit_folder = true;
          break;
#ifdef USE_NNTP
        case 'g': /* Specify a news server */
          cli_nntp = optarg;
          /* fallthrough */
        case 'G': /* List of newsgroups */
          flags |= MUTT_CLI_SELECT | MUTT_CLI_NEWS;
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
          new_type = optarg;
          break;
        case 'n':
          flags |= MUTT_CLI_NOSYSRC;
          break;
        case 'p':
          sendflags |= SEND_POSTPONED;
          break;
        case 'Q':
          mutt_list_insert_tail(&queries, mutt_str_dup(optarg));
          break;
        case 'R':
          flags |= MUTT_CLI_RO; /* read-only mode */
          break;
        case 'S':
          hide_sensitive = true;
          break;
        case 's':
          subject = optarg;
          break;
#ifdef USE_DEBUG_PARSE_TEST
        case 'T':
          test_config = true;
          break;
#endif
        case 'v':
          version++;
          break;
        case 'y': /* My special hack mode */
          flags |= MUTT_CLI_SELECT;
          break;
        case 'Z':
          flags |= MUTT_CLI_MAILBOX | MUTT_CLI_IGNORE;
          break;
        case 'z':
          flags |= MUTT_CLI_IGNORE;
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
      print_version(stdout);
    else
      print_copyright();
    OptNoCurses = true;
    goto main_ok; // TEST04: neomutt -v
  }

  mutt_str_replace(&Username, mutt_str_getenv("USER"));
  mutt_str_replace(&HomeDir, mutt_str_getenv("HOME"));

  cs = init_config(500);
  if (!cs)
    goto main_curses;
  NeoMutt = neomutt_new(cs);

#ifdef USE_DEBUG_NOTIFY
  notify_observer_add(NeoMutt->notify, debug_notify_observer, NULL);
#endif

  if (!get_user_info(cs))
    goto main_exit;

#ifdef USE_DEBUG_PARSE_TEST
  if (test_config)
  {
    cs_str_initial_set(cs, "from", "rich@flatcap.org", NULL);
    cs_str_reset(cs, "from", NULL);
    myvar_set("my_var", "foo");
    test_parse_set();
    goto main_ok;
  }
#endif

  reset_tilde(cs);

  if (dfile)
  {
    cs_str_initial_set(cs, "debug_file", dfile, NULL);
    cs_str_reset(cs, "debug_file", NULL);
  }

  if (dlevel)
  {
    short num = 0;
    if ((mutt_str_atos(dlevel, &num) < 0) || (num < LL_MESSAGE) || (num >= LL_MAX))
    {
      mutt_error(_("Error: value '%s' is invalid for -d"), dlevel);
      goto main_exit; // TEST07: neomutt -d xyz
    }
    cs_str_initial_set(cs, "debug_level", dlevel, NULL);
    cs_str_reset(cs, "debug_level", NULL);
  }

  mutt_log_prep();
  if (dlevel)
    mutt_log_start();

  MuttLogger = log_disp_queue;

  log_translation();

  if (!STAILQ_EMPTY(&cc_list) || !STAILQ_EMPTY(&bcc_list))
  {
    e = email_new();
    e->env = mutt_env_new();

    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &bcc_list, entries)
    {
      mutt_addrlist_parse(&e->env->bcc, np->data);
    }

    STAILQ_FOREACH(np, &cc_list, entries)
    {
      mutt_addrlist_parse(&e->env->cc, np->data);
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
    mutt_window_set_root(COLS, LINES);
  }

  /* set defaults and read init files */
  int rc2 = mutt_init(cs, flags & MUTT_CLI_NOSYSRC, &commands);
  mutt_list_free(&commands);
  if (rc2 != 0)
    goto main_curses;

  mutt_init_abort_key();

  /* The command line overrides the config */
  if (dlevel)
    cs_str_reset(cs, "debug_level", NULL);
  if (dfile)
    cs_str_reset(cs, "debug_file", NULL);

  if (mutt_log_start() < 0)
  {
    mutt_perror("log file");
    goto main_exit;
  }

#ifdef USE_NNTP
  /* "$news_server" precedence: command line, config file, environment, system file */
  if (cli_nntp)
    cs_str_string_set(cs, "news_server", cli_nntp, NULL);
  if (!C_NewsServer)
  {
    const char *env_nntp = mutt_str_getenv("NNTPSERVER");
    cs_str_string_set(cs, "news_server", env_nntp, NULL);
  }
  if (!C_NewsServer)
  {
    char buf[1024];
    char *server = mutt_file_read_keyword(SYSCONFDIR "/nntpserver", buf, sizeof(buf));
    cs_str_string_set(cs, "news_server", server, NULL);
  }
  if (C_NewsServer)
    cs_str_initial_set(cs, "news_server", C_NewsServer, NULL);
#endif

  /* Initialize crypto backends.  */
  crypt_init();

  if (new_type)
  {
    struct Buffer err = mutt_buffer_make(0);
    int r = cs_str_initial_set(cs, "mbox_type", new_type, &err);
    if (CSR_RESULT(r) != CSR_SUCCESS)
    {
      mutt_error(err.data);
      mutt_buffer_dealloc(&err);
      goto main_curses;
    }
    cs_str_reset(cs, "mbox_type", NULL);
  }

  if (!STAILQ_EMPTY(&queries))
  {
    rc = mutt_query_variables(&queries);
    goto main_curses;
  }

  if (dump_variables)
  {
    dump_config(cs, hide_sensitive ? CS_DUMP_HIDE_SENSITIVE : CS_DUMP_NO_FLAGS, stdout);
    goto main_ok; // TEST18: neomutt -D
  }

  if (!STAILQ_EMPTY(&alias_queries))
  {
    rc = 0;
    for (; optind < argc; optind++)
      mutt_list_insert_tail(&alias_queries, mutt_str_dup(argv[optind]));
    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &alias_queries, entries)
    {
      struct AddressList *al = alias_lookup(np->data);
      if (al)
      {
        /* output in machine-readable form */
        mutt_addrlist_to_intl(al, NULL);
        mutt_addrlist_write_file(al, stdout, 0, false);
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
    mutt_curses_set_color(MT_COLOR_NORMAL);
    clear();
    MuttLogger = log_disp_curses;
    log_queue_flush(log_disp_curses);
    log_queue_set_max_size(100);
  }

#ifdef USE_AUTOCRYPT
  /* Initialize autocrypt after curses messages are working,
   * because of the initial account setup screens. */
  if (C_Autocrypt)
    mutt_autocrypt_init(!(sendflags & SEND_BATCH));
#endif

  /* Create the C_Folder directory if it doesn't exist. */
  if (!OptNoCurses && C_Folder)
  {
    struct stat sb;
    struct Buffer *fpath = mutt_buffer_pool_get();

    mutt_buffer_strcpy(fpath, C_Folder);
    mutt_buffer_expand_path(fpath);
    bool skip = false;
#ifdef USE_IMAP
    /* we're not connected yet - skip mail folder creation */
    skip |= (imap_path_probe(mutt_b2s(fpath), NULL) == MUTT_IMAP);
#endif
#ifdef USE_NNTP
    skip |= (nntp_path_probe(mutt_b2s(fpath), NULL) == MUTT_NNTP);
#endif
    if (!skip && (stat(mutt_b2s(fpath), &sb) == -1) && (errno == ENOENT))
    {
      char msg2[256];
      snprintf(msg2, sizeof(msg2), _("%s does not exist. Create it?"), C_Folder);
      if (mutt_yesorno(msg2, MUTT_YES) == MUTT_YES)
      {
        if ((mkdir(mutt_b2s(fpath), 0700) == -1) && (errno != EEXIST))
          mutt_error(_("Can't create %s: %s"), C_Folder, strerror(errno)); // TEST21: neomutt -n -F /dev/null (and ~/Mail doesn't exist)
      }
    }
    mutt_buffer_pool_release(&fpath);
  }

  if (batch_mode)
  {
    goto main_ok; // TEST22: neomutt -B
  }

  notify_observer_add(NeoMutt->notify, mutt_hist_observer, NULL);
  notify_observer_add(NeoMutt->notify, mutt_log_observer, NULL);
  notify_observer_add(NeoMutt->notify, mutt_menu_config_observer, NULL);
  notify_observer_add(NeoMutt->notify, mutt_reply_observer, NULL);
  notify_observer_add(NeoMutt->notify, mutt_abort_key_config_observer, NULL);
  if (Colors)
    notify_observer_add(Colors->notify, mutt_menu_color_observer, NULL);

  if (sendflags & SEND_POSTPONED)
  {
    if (!OptNoCurses)
      mutt_flushinp();
    if (mutt_send_message(SEND_POSTPONED, NULL, NULL, NULL, NULL, NeoMutt->sub) == 0)
      rc = 0;
    // TEST23: neomutt -p (postponed message, cancel)
    // TEST24: neomutt -p (no postponed message)
    log_queue_empty();
    repeat_error = true;
    goto main_curses;
  }
  else if (subject || e || sendflags || draft_file || include_file ||
           !STAILQ_EMPTY(&attach) || (optind < argc))
  {
    FILE *fp_in = NULL;
    FILE *fp_out = NULL;
    char *infile = NULL;
    char *bodytext = NULL;
    const char *bodyfile = NULL;
    int rv = 0;

    if (!OptNoCurses)
      mutt_flushinp();

    if (!e)
      e = email_new();
    if (!e->env)
      e->env = mutt_env_new();

    for (i = optind; i < argc; i++)
    {
      if (url_check_scheme(argv[i]) == U_MAILTO)
      {
        if (!mutt_parse_mailto(e->env, &bodytext, argv[i]))
        {
          mutt_error(_("Failed to parse mailto: link"));
          email_free(&e);
          goto main_curses; // TEST25: neomutt mailto:?
        }
      }
      else
        mutt_addrlist_parse(&e->env->to, argv[i]);
    }

    if (!draft_file && C_Autoedit && TAILQ_EMPTY(&e->env->to) &&
        TAILQ_EMPTY(&e->env->cc))
    {
      mutt_error(_("No recipients specified"));
      email_free(&e);
      goto main_curses; // TEST26: neomutt -s test (with autoedit=yes)
    }

    if (subject)
      e->env->subject = mutt_str_dup(subject);

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
      /* Prepare fp_in and expanded_infile. */
      if (infile)
      {
        if (mutt_str_equal("-", infile))
        {
          if (edit_infile)
          {
            mutt_error(_("Can't use -E flag with stdin"));
            email_free(&e);
            goto main_curses; // TEST27: neomutt -E -H -
          }
          fp_in = stdin;
        }
        else
        {
          mutt_buffer_strcpy(&expanded_infile, infile);
          mutt_buffer_expand_path(&expanded_infile);
          fp_in = fopen(mutt_b2s(&expanded_infile), "r");
          if (!fp_in)
          {
            mutt_perror(mutt_b2s(&expanded_infile));
            email_free(&e);
            goto main_curses; // TEST28: neomutt -E -H missing
          }
        }
      }

      /* Copy input to a tempfile, and re-point fp_in to the tempfile.
       * Note: stdin is always copied to a tempfile, ensuring draft_file
       * can stat and get the correct st_size below.  */
      if (!edit_infile)
      {
        mutt_buffer_mktemp(&tempfile);

        fp_out = mutt_file_fopen(mutt_b2s(&tempfile), "w");
        if (!fp_out)
        {
          mutt_file_fclose(&fp_in);
          mutt_perror(mutt_b2s(&tempfile));
          email_free(&e);
          goto main_curses; // TEST29: neomutt -H existing-file (where tmpdir=/path/to/FILE blocking tmpdir)
        }
        if (fp_in)
        {
          mutt_file_copy_stream(fp_in, fp_out);
          if (fp_in != stdin)
            mutt_file_fclose(&fp_in);
        }
        else if (bodytext)
          fputs(bodytext, fp_out);
        mutt_file_fclose(&fp_out);

        fp_in = fopen(mutt_b2s(&tempfile), "r");
        if (!fp_in)
        {
          mutt_perror(mutt_b2s(&tempfile));
          email_free(&e);
          goto main_curses; // TEST30: can't test
        }
      }
      /* If editing the infile, keep it around afterwards so
       * it doesn't get unlinked, and we can rebuild the draft_file */
      else
        sendflags |= SEND_NO_FREE_HEADER;

      /* Parse the draft_file into the full Email/Body structure.
       * Set SEND_DRAFT_FILE so mutt_send_message doesn't overwrite
       * our e->content.  */
      if (draft_file)
      {
        struct Envelope *opts_env = e->env;
        struct stat st;

        sendflags |= SEND_DRAFT_FILE;

        /* Set up a tmp Email with just enough information so that
         * mutt_prepare_template() can parse the message in fp_in.  */
        struct Email *e_tmp = email_new();
        e_tmp->offset = 0;
        e_tmp->content = mutt_body_new();
        if (fstat(fileno(fp_in), &st) != 0)
        {
          mutt_perror(draft_file);
          email_free(&e);
          email_free(&e_tmp);
          goto main_curses; // TEST31: can't test
        }
        e_tmp->content->length = st.st_size;

        if (mutt_prepare_template(fp_in, NULL, e, e_tmp, false) < 0)
        {
          mutt_error(_("Can't parse message template: %s"), draft_file);
          email_free(&e);
          email_free(&e_tmp);
          goto main_curses;
        }

        /* Scan for neomutt header to set C_ResumeDraftFiles */
        struct ListNode *np = NULL, *tmp = NULL;
        STAILQ_FOREACH_SAFE(np, &e->env->userhdrs, entries, tmp)
        {
          if (mutt_istr_startswith(np->data, "X-Mutt-Resume-Draft:"))
          {
            if (C_ResumeEditedDraftFiles)
              cs_str_native_set(cs, "resume_draft_files", true, NULL);

            STAILQ_REMOVE(&e->env->userhdrs, np, ListNode, entries);
            FREE(&np->data);
            FREE(&np);
          }
        }

        mutt_addrlist_copy(&e->env->to, &opts_env->to, false);
        mutt_addrlist_copy(&e->env->cc, &opts_env->cc, false);
        mutt_addrlist_copy(&e->env->bcc, &opts_env->bcc, false);
        if (opts_env->subject)
          mutt_str_replace(&e->env->subject, opts_env->subject);

        mutt_env_free(&opts_env);
        email_free(&e_tmp);
      }
      /* Editing the include_file: pass it directly in.
       * Note that SEND_NO_FREE_HEADER is set above so it isn't unlinked.  */
      else if (edit_infile)
        bodyfile = mutt_b2s(&expanded_infile);
      // For bodytext and unedited include_file: use the tempfile.
      else
        bodyfile = mutt_b2s(&tempfile);

      mutt_file_fclose(&fp_in);
    }

    FREE(&bodytext);

    if (!STAILQ_EMPTY(&attach))
    {
      struct Body *b = e->content;

      while (b && b->next)
        b = b->next;

      struct ListNode *np = NULL;
      STAILQ_FOREACH(np, &attach, entries)
      {
        if (b)
        {
          b->next = mutt_make_file_attach(np->data, NeoMutt->sub);
          b = b->next;
        }
        else
        {
          b = mutt_make_file_attach(np->data, NeoMutt->sub);
          e->content = b;
        }
        if (!b)
        {
          mutt_error(_("%s: unable to attach file"), np->data);
          mutt_list_free(&attach);
          email_free(&e);
          goto main_curses; // TEST32: neomutt john@example.com -a missing
        }
      }
      mutt_list_free(&attach);
    }

    rv = mutt_send_message(sendflags, e, bodyfile, NULL, NULL, NeoMutt->sub);
    /* We WANT the "Mail sent." and any possible, later error */
    log_queue_empty();
    if (ErrorBufMessage)
      mutt_message("%s", ErrorBuf);

    if (edit_infile)
    {
      if (include_file)
        e->content->unlink = false;
      else if (draft_file)
      {
        if (truncate(mutt_b2s(&expanded_infile), 0) == -1)
        {
          mutt_perror(mutt_b2s(&expanded_infile));
          email_free(&e);
          goto main_curses; // TEST33: neomutt -H read-only -s test john@example.com -E
        }
        fp_out = mutt_file_fopen(mutt_b2s(&expanded_infile), "a");
        if (!fp_out)
        {
          mutt_perror(mutt_b2s(&expanded_infile));
          email_free(&e);
          goto main_curses; // TEST34: can't test
        }

        /* If the message was sent or postponed, these will already
         * have been done.  */
        if (rv < 0)
        {
          if (e->content->next)
            e->content = mutt_make_multipart(e->content);
          mutt_encode_descriptions(e->content, true, NeoMutt->sub);
          mutt_prepare_envelope(e->env, false, NeoMutt->sub);
          mutt_env_to_intl(e->env, NULL, NULL);
        }

        mutt_rfc822_write_header(
            fp_out, e->env, e->content, MUTT_WRITE_HEADER_POSTPONE, false,
            C_CryptProtectedHeadersRead && mutt_should_hide_protected_subject(e),
            NeoMutt->sub);
        if (C_ResumeEditedDraftFiles)
          fprintf(fp_out, "X-Mutt-Resume-Draft: 1\n");
        fputc('\n', fp_out);
        if ((mutt_write_mime_body(e->content, fp_out, NeoMutt->sub) == -1))
        {
          mutt_file_fclose(&fp_out);
          email_free(&e);
          goto main_curses; // TEST35: can't test
        }
        mutt_file_fclose(&fp_out);
      }

      email_free(&e);
    }

    /* !edit_infile && draft_file will leave the tempfile around */
    if (!mutt_buffer_is_empty(&tempfile))
      unlink(mutt_b2s(&tempfile));

    mutt_window_free_all();

    if (rv != 0)
      goto main_curses; // TEST36: neomutt -H existing -s test john@example.com -E (cancel sending)
  }
  else
  {
    if (flags & MUTT_CLI_MAILBOX)
    {
#ifdef USE_IMAP
      bool passive = C_ImapPassive;
      C_ImapPassive = false;
#endif
      if (mutt_mailbox_check(Context ? Context->mailbox : NULL, 0) == 0)
      {
        mutt_message(_("No mailbox with new mail"));
        goto main_curses; // TEST37: neomutt -Z (no new mail)
      }
      mutt_buffer_reset(&folder);
      mutt_mailbox_next(Context ? Context->mailbox : NULL, &folder);
#ifdef USE_IMAP
      C_ImapPassive = passive;
#endif
    }
    else if (flags & MUTT_CLI_SELECT)
    {
#ifdef USE_NNTP
      if (flags & MUTT_CLI_NEWS)
      {
        OptNews = true;
        CurrentNewsSrv =
            nntp_select_server(Context ? Context->mailbox : NULL, C_NewsServer, false);
        if (!CurrentNewsSrv)
          goto main_curses; // TEST38: neomutt -G (unset news_server)
      }
      else
#endif
          if (TAILQ_EMPTY(&NeoMutt->accounts))
      {
        mutt_error(_("No incoming mailboxes defined"));
        goto main_curses; // TEST39: neomutt -n -F /dev/null -y
      }
      mutt_buffer_reset(&folder);
      mutt_buffer_select_file(&folder, MUTT_SEL_FOLDER | MUTT_SEL_MAILBOX, NULL, NULL);
      if (mutt_buffer_is_empty(&folder))
      {
        goto main_ok; // TEST40: neomutt -y (quit selection)
      }
    }

    if (mutt_buffer_is_empty(&folder))
    {
      if (C_Spoolfile)
      {
        // Check if C_Spoolfile corresponds a mailboxes' description.
        struct Mailbox *m_desc = mailbox_find_name(C_Spoolfile);
        if (m_desc)
          mutt_buffer_strcpy(&folder, m_desc->realpath);
        else
          mutt_buffer_strcpy(&folder, C_Spoolfile);
      }
      else if (C_Folder)
        mutt_buffer_strcpy(&folder, C_Folder);
      /* else no folder */
    }

#ifdef USE_NNTP
    if (OptNews)
    {
      OptNews = false;
      mutt_buffer_alloc(&folder, PATH_MAX);
      nntp_expand_path(folder.data, folder.dsize, &CurrentNewsSrv->conn->account);
    }
    else
#endif
      mutt_buffer_expand_path(&folder);

    mutt_str_replace(&CurrentFolder, mutt_b2s(&folder));
    mutt_str_replace(&LastFolder, mutt_b2s(&folder));

    if (flags & MUTT_CLI_IGNORE)
    {
      /* check to see if there are any messages in the folder */
      switch (mx_check_empty(mutt_b2s(&folder)))
      {
        case -1:
          mutt_perror(mutt_b2s(&folder));
          goto main_curses; // TEST41: neomutt -z -f missing
        case 1:
          mutt_error(_("Mailbox is empty"));
          goto main_curses; // TEST42: neomutt -z -f /dev/null
      }
    }

    mutt_folder_hook(mutt_b2s(&folder), NULL);
    mutt_startup_shutdown_hook(MUTT_STARTUP_HOOK);
    notify_send(NeoMutt->notify, NT_GLOBAL, NT_GLOBAL_STARTUP, NULL);

    repeat_error = true;
    struct Mailbox *m = mx_resolve(mutt_b2s(&folder));
    Context = mx_mbox_open(m, ((flags & MUTT_CLI_RO) || C_ReadOnly) ? MUTT_READONLY : MUTT_OPEN_NO_FLAGS);
    if (!Context)
    {
      if (m->account)
        account_mailbox_remove(m->account, m);

      mailbox_free(&m);
      mutt_error(_("Unable to open mailbox %s"), mutt_b2s(&folder));
      repeat_error = false;
    }
    if (Context || !explicit_folder)
    {
#ifdef USE_SIDEBAR
      sb_set_open_mailbox(Context ? Context->mailbox : NULL);
#endif
      struct MuttWindow *dlg = index_pager_init();
      dialog_push(dlg);
      mutt_index_menu(dlg);
      dialog_pop();
      index_pager_shutdown(dlg);
      mutt_window_free(&dlg);
      ctx_free(&Context);
      log_queue_empty();
      repeat_error = false;
    }
#ifdef USE_IMAP
    imap_logout_all();
#endif
#ifdef USE_SASL
    mutt_sasl_done();
#endif
#ifdef USE_AUTOCRYPT
    mutt_autocrypt_cleanup();
#endif
    // TEST43: neomutt (no change to mailbox)
    // TEST44: neomutt (change mailbox)
  }

main_ok:
  rc = 0;
main_curses:
  mutt_endwin();
  mutt_unlink_temp_attachments();
  /* Repeat the last message to the user */
  if (repeat_error && ErrorBufMessage)
    puts(ErrorBuf);
main_exit:
  MuttLogger = log_disp_queue;
  mutt_buffer_dealloc(&folder);
  mutt_buffer_dealloc(&expanded_infile);
  mutt_buffer_dealloc(&tempfile);
  mutt_list_free(&queries);
  crypto_module_free();
  mutt_window_free_all();
  mutt_buffer_pool_free();
  mutt_envlist_free();
  mutt_browser_cleanup();
  mutt_commands_cleanup();
  crypt_cleanup();
  mutt_opts_free();
  mutt_keys_free();
  myvarlist_free(&MyVars);
  mutt_prex_free();
  neomutt_free(&NeoMutt);
  cs_free(&cs);
  log_queue_flush(log_disp_terminal);
  log_queue_empty();
  mutt_log_stop();
  return rc;
}
