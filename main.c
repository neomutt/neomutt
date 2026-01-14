/**
 * @file
 * Command line processing
 *
 * @authors
 * Copyright (C) 1996-2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2007 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2004 g10 Code GmbH
 * Copyright (C) 2016-2026 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2017-2025 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
 * Copyright (C) 2024-2025 Alejandro Colomar <alx@kernel.org>
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
 * @mainpage Code Docs
 *
 * <img style="float: left; padding-right: 0.5em;" src="structs.svg">
 * <img style="float: left; padding-right: 0.5em;" src="pages.svg">
 * <img style="float: left; padding-right: 0.5em;" src="globals.svg">
 * <img style="float: left; padding-right: 0.5em;" src="functions.svg">
 * <img style="float: left; padding-right: 0.5em;" src="enums.svg">
 * <img style="float: left; padding-right: 0.5em;" src="members.svg">
 * <img style="float: left; padding-right: 0.5em;" src="defines.svg">
 * <br>
 *
 * ## Libraries
 *
 * [Each library](pages.html) helps to untangle the code by grouping similar
 * functions and reducing dependencies.
 *
 * The goal is that each library is:
 * - Self-contained (it may rely on other libraries)
 * - Independently testable (i.e. without using NeoMutt)
 * - Fully documented
 * - Robust
 *
 * ## Miscellaneous files
 *
 * These file form the main body of NeoMutt.
 *
 * | File            | Description                |
 * | :-------------- | :------------------------- |
 * | editmsg.c       | @subpage neo_editmsg       |
 * | external.c      | @subpage neo_external      |
 * | flags.c         | @subpage neo_flags         |
 * | globals.c       | @subpage neo_globals       |
 * | help.c          | @subpage neo_help          |
 * | main.c          | @subpage neo_main          |
 * | module.c        | @subpage main_module       |
 * | monitor.c       | @subpage neo_monitor       |
 * | muttlib.c       | @subpage neo_muttlib       |
 * | mutt_config.c   | @subpage neo_mutt_config   |
 * | mutt_logging.c  | @subpage neo_mutt_logging  |
 * | mutt_mailbox.c  | @subpage neo_mutt_mailbox  |
 * | mutt_signal.c   | @subpage neo_mutt_signal   |
 * | mutt_socket.c   | @subpage neo_mutt_socket   |
 * | mx.c            | @subpage neo_mx            |
 * | system.c        | @subpage neo_system        |
 * | usage.c         | @subpage neo_usage         |
 * | version.c       | @subpage neo_version       |
 *
 * ## Building these Docs
 *
 * The config for building the docs is in the main source repo.
 *
 * Everything possible is turned on in the config file, so you'll need to
 * install a few dependencies like `dot` from the graphviz package.
 *
 * ## Installing the Docs
 *
 * These docs aren't in the main website repo -- they weigh in at 100MB.
 * Instead, they're stored in the [code repo](https://github.com/neomutt/code)
 */

/**
 * @page neo_main Command line processing
 *
 * Command line processing
 */

#define GNULIB_defined_setlocale

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "attach/lib.h"
#include "browser/lib.h"
#include "cli/lib.h"
#include "color/lib.h"
#include "commands/lib.h"
#include "compose/lib.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "hooks/lib.h"
#include "imap/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "lua/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "nntp/lib.h"
#include "pager/lib.h"
#include "parse/lib.h"
#include "pop/lib.h"
#include "postpone/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "sidebar/lib.h"
#include "external.h"
#include "globals.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "muttlib.h"
#include "mx.h"
#include "nntp/adata.h" // IWYU pragma: keep
#include "version.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif
#if defined(USE_DEBUG_NOTIFY) || defined(USE_DEBUG_BACKTRACE)
#include "debug/lib.h"
#endif
#ifndef DOMAIN
#include "conn/lib.h"
#endif

bool StartupComplete = false; ///< When the config has been read

void show_cli(enum HelpMode mode, bool use_color);

// clang-format off
extern const struct Module ModuleMain;
extern const struct Module ModuleAddress;  extern const struct Module ModuleAlias;    extern const struct Module ModuleAttach;   extern const struct Module ModuleAutocrypt;
extern const struct Module ModuleBcache;   extern const struct Module ModuleBrowser;  extern const struct Module ModuleColor;    extern const struct Module ModuleCommands;
extern const struct Module ModuleComplete; extern const struct Module ModuleCompmbox; extern const struct Module ModuleCompose;  extern const struct Module ModuleCompress;
extern const struct Module ModuleConfig;   extern const struct Module ModuleConn;     extern const struct Module ModuleConvert;  extern const struct Module ModuleCore;
extern const struct Module ModuleEditor;   extern const struct Module ModuleEmail;    extern const struct Module ModuleEnvelope; extern const struct Module ModuleExpando;
extern const struct Module ModuleGui;      extern const struct Module ModuleHcache;   extern const struct Module ModuleHelpbar;  extern const struct Module ModuleHistory;
extern const struct Module ModuleHooks;    extern const struct Module ModuleImap;     extern const struct Module ModuleIndex;    extern const struct Module ModuleKey;
extern const struct Module ModuleLua;      extern const struct Module ModuleMaildir;  extern const struct Module ModuleMbox;     extern const struct Module ModuleMenu;
extern const struct Module ModuleMh;       extern const struct Module ModuleMutt;     extern const struct Module ModuleNcrypt;   extern const struct Module ModuleNntp;
extern const struct Module ModuleNotmuch;  extern const struct Module ModulePager;    extern const struct Module ModuleParse;    extern const struct Module ModulePattern;
extern const struct Module ModulePop;      extern const struct Module ModulePostpone; extern const struct Module ModuleProgress; extern const struct Module ModuleQuestion;
extern const struct Module ModuleSend;     extern const struct Module ModuleSidebar;  extern const struct Module ModuleStore;
// clang-format on

/**
 * Modules - All the library Modules
 */
static const struct Module *Modules[] = {
  // clang-format off
  &ModuleMain,     &ModuleGui,      // These two have priority
  &ModuleAddress,  &ModuleAlias,    &ModuleAttach,   &ModuleBcache,   &ModuleBrowser,
  &ModuleColor,    &ModuleCommands, &ModuleComplete, &ModuleCompmbox, &ModuleCompose,
  &ModuleConfig,   &ModuleConn,     &ModuleConvert,  &ModuleCore,     &ModuleEditor,
  &ModuleEmail,    &ModuleEnvelope, &ModuleExpando,  &ModuleHelpbar,  &ModuleHistory,
  &ModuleHooks,    &ModuleImap,     &ModuleIndex,    &ModuleKey,      &ModuleMaildir,
  &ModuleMbox,     &ModuleMenu,     &ModuleMh,       &ModuleMutt,     &ModuleNcrypt,
  &ModuleNntp,     &ModulePager,    &ModuleParse,    &ModulePattern,  &ModulePop,
  &ModulePostpone, &ModuleProgress, &ModuleQuestion, &ModuleSend,     &ModuleSidebar,
// clang-format on
#ifdef USE_AUTOCRYPT
  &ModuleAutocrypt,
#endif
#ifdef USE_HCACHE_COMPRESSION
  &ModuleCompress,
#endif
#ifdef USE_HCACHE
  &ModuleHcache,
#endif
#ifdef USE_LUA
  &ModuleLua,
#endif
#ifdef USE_NOTMUCH
  &ModuleNotmuch,
#endif
#ifdef USE_HCACHE
  &ModuleStore,
#endif
  NULL,
};

/**
 * execute_commands - Execute a set of NeoMutt commands
 * @param sa Array of command strings
 * @retval  0 Success, all the commands succeeded
 * @retval -1 Error
 */
static int execute_commands(struct StringArray *sa)
{
  int rc = 0;
  struct Buffer *err = buf_pool_get();
  struct Buffer *line = buf_pool_get();

  const char **cp = NULL;
  ARRAY_FOREACH(cp, sa)
  {
    buf_strcpy(line, *cp);
    enum CommandResult rc2 = parse_rc_line(line, err);
    if (rc2 == MUTT_CMD_ERROR)
      mutt_error(_("Error in command line: %s"), buf_string(err));
    else if (rc2 == MUTT_CMD_WARNING)
      mutt_warning(_("Warning in command line: %s"), buf_string(err));

    if ((rc2 == MUTT_CMD_ERROR) || (rc2 == MUTT_CMD_WARNING))
    {
      buf_pool_release(&err);
      return -1;
    }
  }

  buf_pool_release(&line);
  buf_pool_release(&err);

  return rc;
}

/**
 * find_cfg - Find a config file
 * @param home         User's home directory
 * @param xdg_cfg_home XDG home directory
 * @retval ptr  Success, first matching directory
 * @retval NULL Error, no matching directories
 */
static char *find_cfg(const char *home, const char *xdg_cfg_home)
{
  const char *names[] = {
    "neomuttrc",
    "muttrc",
    NULL,
  };

  const char *locations[][2] = {
    { xdg_cfg_home, "neomutt/" },
    { xdg_cfg_home, "mutt/" },
    { home, ".neomutt/" },
    { home, ".mutt/" },
    { home, "." },
    { NULL, NULL },
  };

  struct Buffer *buf = buf_pool_get();
  char *cfg = NULL;

  for (int i = 0; locations[i][0] || locations[i][1]; i++)
  {
    if (!locations[i][0])
      continue;

    for (int j = 0; names[j]; j++)
    {
      buf_printf(buf, "%s/%s%s", locations[i][0], locations[i][1], names[j]);
      if (access(buf_string(buf), F_OK) == 0)
      {
        cfg = buf_strdup(buf);
        goto done;
      }
    }
  }

done:
  buf_pool_release(&buf);
  return cfg;
}

#ifndef DOMAIN
/**
 * getmailname - Try to retrieve the FQDN from mailname files
 * @retval ptr Heap allocated string with the FQDN
 * @retval NULL No valid mailname file could be read
 */
static char *getmailname(void)
{
  char *mailname = NULL;
  static const char *mn_files[] = { "/etc/mailname", "/etc/mail/mailname" };

  for (size_t i = 0; i < countof(mn_files); i++)
  {
    FILE *fp = mutt_file_fopen(mn_files[i], "r");
    if (!fp)
      continue;

    size_t len = 0;
    mailname = mutt_file_read_line(NULL, &len, fp, NULL, MUTT_RL_NO_FLAGS);
    mutt_file_fclose(&fp);
    if (mailname && *mailname)
      break;

    FREE(&mailname);
  }

  return mailname;
}
#endif

/**
 * get_hostname - Find the Fully-Qualified Domain Name
 * @param cs Config Set
 * @retval true  Success
 * @retval false Error, failed to find any name
 *
 * Use several methods to try to find the Fully-Qualified domain name of this host.
 * If the user has already configured a hostname, this function will use it.
 */
static bool get_hostname(struct ConfigSet *cs)
{
  const char *short_host = NULL;
  struct utsname utsname = { 0 };

  const char *const c_hostname = cs_subset_string(NeoMutt->sub, "hostname");
  if (c_hostname)
  {
    short_host = c_hostname;
  }
  else
  {
    /* The call to uname() shouldn't fail, but if it does, the system is horribly
     * broken, and the system's networking configuration is in an unreliable
     * state.  We should bail.  */
    if ((uname(&utsname)) == -1)
    {
      mutt_perror(_("unable to determine nodename via uname()"));
      return false; // TEST09: can't test
    }

    short_host = utsname.nodename;
  }

  /* some systems report the FQDN instead of just the hostname */
  char *dot = strchr(short_host, '.');
  if (dot)
    ShortHostname = mutt_strn_dup(short_host, dot - short_host);
  else
    ShortHostname = mutt_str_dup(short_host);

  // All the code paths from here alloc memory for the fqdn
  char *fqdn = mutt_str_dup(c_hostname);
  if (!fqdn)
  {
    mutt_debug(LL_DEBUG1, "Setting $hostname\n");
    /* now get FQDN.  Use configured domain first, DNS next, then uname */
#ifdef DOMAIN
    /* we have a compile-time domain name, use that for `$hostname` */
    mutt_str_asprintf(&fqdn, "%s.%s", NONULL(ShortHostname), DOMAIN);
#else
    fqdn = getmailname();
    if (!fqdn)
    {
      struct Buffer *domain = buf_pool_get();
      if (getdnsdomainname(domain) == 0)
      {
        mutt_str_asprintf(&fqdn, "%s.%s", NONULL(ShortHostname), buf_string(domain));
      }
      else
      {
        /* DNS failed, use the nodename.  Whether or not the nodename had a '.'
         * in it, we can use the nodename as the FQDN.  On hosts where DNS is
         * not being used, e.g. small network that relies on hosts files, a
         * short host name is all that is required for SMTP to work correctly.
         * It could be wrong, but we've done the best we can, at this point the
         * onus is on the user to provide the correct hostname if the nodename
         * won't work in their network.  */
        fqdn = mutt_str_dup(utsname.nodename);
      }
      buf_pool_release(&domain);
      mutt_debug(LL_DEBUG1, "Hostname: %s\n", NONULL(fqdn));
    }
#endif
  }

  if (fqdn)
  {
    config_str_set_initial(cs, "hostname", fqdn);
    FREE(&fqdn);
  }

  return true;
}

/**
 * mutt_init - Initialise NeoMutt
 * @param cs          Config Set
 * @param dlevel      Command line debug level
 * @param dfile       Command line debug file
 * @param skip_sys_rc If true, don't read the system config file
 * @param user_files  Array of user config files
 * @param commands    Array of config commands to execute
 * @retval 0 Success
 * @retval 1 Error
 */
static int mutt_init(struct ConfigSet *cs, struct Buffer *dlevel,
                     struct Buffer *dfile, bool skip_sys_rc,
                     struct StringArray *user_files, struct StringArray *commands)
{
  bool need_pause = false;
  int rc = 1;
  struct Buffer *err = buf_pool_get();
  struct Buffer *buf = buf_pool_get();
  const char **cp = NULL;

#ifdef NEOMUTT_DIRECT_COLORS
  /* Test if we run in a terminal which supports direct colours.
   *
   * The user/terminal can indicate their capability independent of the
   * terminfo file by setting the COLORTERM environment variable to "truecolor"
   * or "24bit" (case sensitive).
   *
   * Note: This is to test is less about whether the terminal understands
   * direct color commands but more about whether ncurses believes it can send
   * them to the terminal, e.g. ncurses ignores COLORTERM.
   */
  if (COLORS == 16777216) // 2^24
  {
    /* Ncurses believes the Terminal supports it check the environment variable
     * to respect the user's choice */
    const char *env_colorterm = mutt_str_getenv("COLORTERM");
    if (env_colorterm && (mutt_str_equal(env_colorterm, "truecolor") ||
                          mutt_str_equal(env_colorterm, "24bit")))
    {
      config_str_set_initial(cs, "color_directcolor", "yes");
    }
  }
#endif

  /* "$spool_file" precedence: config file, environment */
  const char *p = mutt_str_getenv("MAIL");
  if (!p)
    p = mutt_str_getenv("MAILDIR");
  if (!p)
  {
#ifdef HOMESPOOL
    buf_concat_path(buf, NONULL(NeoMutt->home_dir), MAILPATH);
#else
    buf_concat_path(buf, MAILPATH, NONULL(NeoMutt->username));
#endif
    p = buf_string(buf);
  }
  config_str_set_initial(cs, "spool_file", p);

  p = mutt_str_getenv("REPLYTO");
  if (p)
  {
    buf_printf(buf, "Reply-To: %s", p);
    buf_seek(buf, 0);

    // Create a temporary Command struct for parse_my_hdr
    const struct Command my_hdr_cmd = { .name = "my-header", .data = 0 };
    parse_my_header(&my_hdr_cmd, buf, err); /* adds to UserHeader */
  }

  p = mutt_str_getenv("EMAIL");
  if (p)
    config_str_set_initial(cs, "from", p);

  /* "$mailcap_path" precedence: config file, environment, code */
  struct Buffer *mc = buf_pool_get();
  struct Slist *sl_mc = NULL;
  const char *env_mc = mutt_str_getenv("MAILCAPS");
  if (env_mc)
  {
    sl_mc = slist_parse(env_mc, D_SLIST_SEP_COLON);
  }
  else
  {
    cs_str_initial_get(cs, "mailcap_path", mc);
    sl_mc = slist_parse(buf_string(mc), D_SLIST_SEP_COLON);
    buf_reset(mc);
  }
  slist_to_buffer(sl_mc, mc);
  config_str_set_initial(cs, "mailcap_path", buf_string(mc));
  slist_free(&sl_mc);
  buf_pool_release(&mc);

  /* "$tmp_dir" precedence: config file, environment, code */
  const char *env_tmp = mutt_str_getenv("TMPDIR");
  if (env_tmp)
    config_str_set_initial(cs, "tmp_dir", env_tmp);

  /* "$visual", "$editor" precedence: config file, environment, code */
  const char *env_ed = mutt_str_getenv("VISUAL");
  if (!env_ed)
    env_ed = mutt_str_getenv("EDITOR");
  if (!env_ed)
    env_ed = "vi";
  config_str_set_initial(cs, "editor", env_ed);

  const char *charset = mutt_ch_get_langinfo_charset();
  config_str_set_initial(cs, "charset", charset);
  mutt_ch_set_charset(charset);
  FREE(&charset);

  char name[256] = { 0 };
  const char *c_real_name = cs_subset_string(NeoMutt->sub, "real_name");
  if (!c_real_name)
  {
    struct passwd *pw = getpwuid(getuid());
    if (pw)
    {
      c_real_name = mutt_gecos_name(name, sizeof(name), pw);
    }
  }
  config_str_set_initial(cs, "real_name", c_real_name);

#ifdef HAVE_GETSID
  /* Unset suspend by default if we're the session leader */
  if (getsid(0) == getpid())
    config_str_set_initial(cs, "suspend", "no");
#endif

  /* RFC2368, "4. Unsafe headers"
   * The creator of a mailto URL can't expect the resolver of a URL to
   * understand more than the "subject" and "body" headers. Clients that
   * resolve mailto URLs into mail messages should be able to correctly
   * create RFC822-compliant mail messages using the "subject" and "body"
   * headers.  */
  add_to_stailq(&MailToAllow, "body");
  add_to_stailq(&MailToAllow, "subject");
  /* Cc, In-Reply-To, and References help with not breaking threading on
   * mailing lists, see https://github.com/neomutt/neomutt/issues/115 */
  add_to_stailq(&MailToAllow, "cc");
  add_to_stailq(&MailToAllow, "in-reply-to");
  add_to_stailq(&MailToAllow, "references");

  if (ARRAY_EMPTY(user_files))
  {
    const char *xdg_cfg_home = mutt_str_getenv("XDG_CONFIG_HOME");

    if (!xdg_cfg_home && NeoMutt->home_dir)
    {
      buf_printf(buf, "%s/.config", NeoMutt->home_dir);
      xdg_cfg_home = buf_string(buf);
    }

    char *config = find_cfg(NeoMutt->home_dir, xdg_cfg_home);
    if (config)
    {
      ARRAY_ADD(user_files, config);
    }
  }
  else
  {
    ARRAY_FOREACH(cp, user_files)
    {
      buf_strcpy(buf, *cp);
      FREE(cp);
      buf_expand_path(buf);
      ARRAY_SET(user_files, ARRAY_FOREACH_IDX_cp, buf_strdup(buf));
      if (access(buf_string(buf), F_OK))
      {
        mutt_perror("%s", buf_string(buf));
        goto done; // TEST10: neomutt -F missing
      }
    }
  }

  ARRAY_FOREACH(cp, user_files)
  {
    if (*cp && !mutt_str_equal(*cp, "/dev/null"))
    {
      cs_str_string_set(cs, "alias_file", *cp, NULL);
      break;
    }
  }

  /* Process the global rc file if it exists and the user hasn't explicitly
   * requested not to via "-n".  */
  if (!skip_sys_rc)
  {
    do
    {
      if (mutt_set_xdg_path(XDG_CONFIG_DIRS, buf))
        break;

      buf_printf(buf, "%s/neomuttrc", SYSCONFDIR);
      if (access(buf_string(buf), F_OK) == 0)
        break;

      buf_printf(buf, "%s/Muttrc", SYSCONFDIR);
      if (access(buf_string(buf), F_OK) == 0)
        break;

      buf_printf(buf, "%s/neomuttrc", PKGDATADIR);
      if (access(buf_string(buf), F_OK) == 0)
        break;

      buf_printf(buf, "%s/Muttrc", PKGDATADIR);
    } while (false);

    if (access(buf_string(buf), F_OK) == 0)
    {
      if (source_rc(buf_string(buf), err) != 0)
      {
        mutt_error("%s", buf_string(err));
        need_pause = true; // TEST11: neomutt (error in /etc/neomuttrc)
      }
    }
  }

  /* Read the user's initialization file.  */
  ARRAY_FOREACH(cp, user_files)
  {
    if (*cp)
    {
      if (source_rc(*cp, err) != 0)
      {
        mutt_error("%s", buf_string(err));
        need_pause = true; // TEST12: neomutt (error in ~/.neomuttrc)
      }
    }
  }

  if (execute_commands(commands) != 0)
    need_pause = true; // TEST13: neomutt -e broken

  if (!get_hostname(cs))
    goto done;

  /* The command line overrides the config */
  if (!buf_is_empty(dlevel))
    cs_str_reset(cs, "debug_level", NULL);
  if (!buf_is_empty(dfile))
    cs_str_reset(cs, "debug_file", NULL);

  if (mutt_log_start() < 0)
  {
    mutt_perror("log file");
    goto done;
  }

  if (need_pause && OptGui)
  {
    log_queue_flush(log_disp_terminal);
    if (mutt_any_key_to_continue(NULL) == 'q')
      goto done; // TEST14: neomutt -e broken (press 'q')
  }

  const char *const c_tmp_dir = cs_subset_path(NeoMutt->sub, "tmp_dir");
  if (mutt_file_mkdir(c_tmp_dir, S_IRWXU) < 0)
  {
    mutt_error(_("Can't create %s: %s"), c_tmp_dir, strerror(errno));
    goto done;
  }

  rc = 0;

done:
  buf_pool_release(&err);
  buf_pool_release(&buf);
  return rc;
}

/**
 * get_elem_queries - Lookup the HashElems for a set of queries
 * @param[in]  queries Array of query strings
 * @param[out] hea     Array for Config HashElems
 * @retval 0 Success, all queries exist
 * @retval 1 Error
 */
static int get_elem_queries(struct StringArray *queries, struct HashElemArray *hea)
{
  int rc = 0;
  const char **cp = NULL;
  ARRAY_FOREACH(cp, queries)
  {
    struct HashElem *he = cs_subset_lookup(NeoMutt->sub, *cp);
    if (!he)
    {
      mutt_warning(_("Unknown option %s"), *cp);
      rc = 1;
      continue;
    }

    if (he->type & D_INTERNAL_DEPRECATED)
    {
      mutt_warning(_("Option %s is deprecated"), *cp);
      rc = 1;
      continue;
    }

    ARRAY_ADD(hea, he);
  }

  return rc; // TEST16: neomutt -Q charset
}

/**
 * init_keys - Initialise the Keybindings
 */
static void init_keys(void)
{
  km_init();

  struct SubMenu *sm_generic = generic_init_keys();

  alias_init_keys(sm_generic);
  attach_init_keys(sm_generic);
#ifdef USE_AUTOCRYPT
  autocrypt_init_keys(sm_generic);
#endif
  browser_init_keys(sm_generic);
  compose_init_keys(sm_generic);
  editor_init_keys(sm_generic);
  sidebar_init_keys(sm_generic);
  index_init_keys(sm_generic);
  pager_init_keys(sm_generic);
  pgp_init_keys(sm_generic);
  postponed_init_keys(sm_generic);
}

/**
 * start_curses - Start the Curses UI
 * @retval 0 Success
 * @retval 1 Failure
 */
static int start_curses(void)
{
  /* should come before initscr() so that ncurses 4.2 doesn't try to install
   * its own SIGWINCH handler */
  mutt_signal_init();

  if (!initscr())
  {
    mutt_error(_("Error initializing terminal"));
    return 1;
  }

  colors_init();
  keypad(stdscr, true);
  cbreak();
  noecho();
  nonl();
  typeahead(-1); /* simulate smooth scrolling */
  meta(stdscr, true);
  ext_keys_init();
  /* Now that curses is set up, we drop back to normal screen mode.
   * This simplifies displaying error messages to the user.
   * The first call to refresh() will swap us back to curses screen mode. */
  endwin();
  return 0;
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

  /* Get some information about the user */
  struct passwd *pw = getpwuid(getuid());
  if (pw)
  {
    if (!NeoMutt->username)
      NeoMutt->username = mutt_str_dup(pw->pw_name);
    if (!NeoMutt->home_dir)
      NeoMutt->home_dir = mutt_str_dup(pw->pw_dir);
    if (!shell)
      shell = pw->pw_shell;
  }

  if (!NeoMutt->username)
  {
    mutt_error(_("unable to determine username"));
    return false; // TEST05: neomutt (unset $USER, delete user from /etc/passwd)
  }

  if (!NeoMutt->home_dir)
  {
    mutt_error(_("unable to determine home directory"));
    return false; // TEST06: neomutt (unset $HOME, delete user from /etc/passwd)
  }

  if (shell)
    config_str_set_initial(cs, "shell", shell);

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
  const char *label = "Language:"; // the start of the lookup/needle
  const char *lang = mutt_istr_find(header, label);
  int len = 64;
  if (lang)
  {
    lang += strlen(label); // skip label
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
 * log_gui - Log info about the GUI
 */
static void log_gui(void)
{
  const char *term = mutt_str_getenv("TERM");
  const char *color_term = mutt_str_getenv("COLORTERM");
  bool true_color = false;
#ifdef NEOMUTT_DIRECT_COLORS
  true_color = true;
#endif

  mutt_debug(LL_DEBUG1, "GUI:\n");
  mutt_debug(LL_DEBUG1, "    Curses: %s\n", curses_version());
  mutt_debug(LL_DEBUG1, "    COLORS=%d\n", COLORS);
  mutt_debug(LL_DEBUG1, "    COLOR_PAIRS=%d\n", COLOR_PAIRS);
  mutt_debug(LL_DEBUG1, "    TERM=%s\n", NONULL(term));
  mutt_debug(LL_DEBUG1, "    COLORTERM=%s\n", NONULL(color_term));
  mutt_debug(LL_DEBUG1, "    True color support: %s\n", true_color ? "YES" : "NO");
  mutt_debug(LL_DEBUG1, "    Screen: %dx%d\n", RootWindow->state.cols,
             RootWindow->state.rows);
}

/**
 * main_timeout_observer - Notification that a timeout has occurred - Implements ::observer_t - @ingroup observer_api
 */
static int main_timeout_observer(struct NotifyCallback *nc)
{
  static time_t last_run = 0;

  if (nc->event_type != NT_TIMEOUT)
    return 0;

  const short c_timeout = cs_subset_number(NeoMutt->sub, "timeout");
  if (c_timeout <= 0)
    goto done;

  time_t now = mutt_date_now();
  if (now < (last_run + c_timeout))
    goto done;

  // Limit hook to running under the Index or Pager
  struct MuttWindow *focus = window_get_focus();
  struct MuttWindow *dlg = dialog_find(focus);
  if (!dlg || (dlg->type != WT_DLG_INDEX))
    goto done;

  last_run = now;
  mutt_timeout_hook();

done:
  mutt_debug(LL_DEBUG5, "timeout done\n");
  return 0;
}

/**
 * show_help - Show the Help
 * @param help Command Line Options
 * @retval true Success, continue
 */
static bool show_help(struct CliHelp *help)
{
  if (!help->is_set)
    return true;

  log_queue_flush(log_disp_terminal);

  const bool tty = isatty(STDOUT_FILENO);

  if (help->help)
  {
    show_cli(help->mode, tty);
  }
  else if (help->license)
  {
    print_copyright();
  }
  else
  {
    print_version(stdout, tty);
  }

  return false; // Stop
}

/**
 * init_logging - Initialise the Logging
 * @param shared Shared Command line Options
 * @param cs     Config Set
 * @retval true Success
 */
static bool init_logging(struct CliShared *shared, struct ConfigSet *cs)
{
  if (!shared->is_set)
    return true;

  if (!buf_is_empty(&shared->log_file))
    config_str_set_initial(cs, "debug_file", buf_string(&shared->log_file));

  if (!buf_is_empty(&shared->log_level))
  {
    const char *dlevel = buf_string(&shared->log_level);
    short num = 0;
    if (!mutt_str_atos_full(dlevel, &num) || (num < LL_MESSAGE) || (num >= LL_MAX))
    {
      mutt_error(_("Error: value '%s' is invalid for -d"), dlevel);
      return false;
    }

    config_str_set_initial(cs, "debug_level", dlevel);
  }

  return true;
}

/**
 * init_nntp - Initialise the NNTP config
 * @param server NNTP Server to use
 * @param cs     Config Set
 */
static void init_nntp(struct Buffer *server, struct ConfigSet *cs)
{
  const char *cli_nntp = NULL;
  if (!buf_is_empty(server))
    cli_nntp = buf_string(server);

  /* "$news_server" precedence: command line, config file, environment, system file */
  if (cli_nntp)
    cli_nntp = cs_subset_string(NeoMutt->sub, "news_server");

  if (!cli_nntp)
    cli_nntp = mutt_str_getenv("NNTPSERVER");

  if (!cli_nntp)
  {
    char buf[1024] = { 0 };
    cli_nntp = mutt_file_read_keyword(SYSCONFDIR "/nntpserver", buf, sizeof(buf));
  }

  if (cli_nntp)
    config_str_set_initial(cs, "news_server", cli_nntp);
}

/**
 * dump_info - Show config info
 * @param ci Command line Options
 * @param cs Config Set
 * @retval true Success
 */
static bool dump_info(struct CliInfo *ci, struct ConfigSet *cs)
{
  if (!ci->is_set)
    return true;

  if (ci->dump_config || !ARRAY_EMPTY(&ci->queries))
  {
    const bool tty = isatty(STDOUT_FILENO);

    ConfigDumpFlags cdflags = CS_DUMP_NO_FLAGS;
    if (tty)
      cdflags |= CS_DUMP_LINK_DOCS;
    if (ci->hide_sensitive)
      cdflags |= CS_DUMP_HIDE_SENSITIVE;
    if (ci->show_help)
      cdflags |= CS_DUMP_SHOW_DOCS;

    struct HashElemArray hea = ARRAY_HEAD_INITIALIZER;
    if (ci->dump_config)
    {
      enum GetElemListFlags gel_flags = ci->dump_changed ? GEL_CHANGED_CONFIG : GEL_ALL_CONFIG;
      hea = get_elem_list(cs, gel_flags);
    }
    else
    {
      get_elem_queries(&ci->queries, &hea);
    }

    dump_config(cs, &hea, cdflags, stdout);
    ARRAY_FREE(&hea);
  }
  else if (!ARRAY_EMPTY(&ci->alias_queries))
  {
    const char **cp = NULL;
    ARRAY_FOREACH(cp, &ci->alias_queries)
    {
      struct AddressList *al = alias_lookup(*cp);
      if (al)
      {
        /* output in machine-readable form */
        mutt_addrlist_to_intl(al, NULL);
        struct Buffer *buf = buf_pool_get();
        mutt_addrlist_write(al, buf, false);
        printf("%s\n", buf_string(buf));
        buf_pool_release(&buf);
      }
      else
      {
        printf("%s\n", NONULL(*cp)); // TEST19: neomutt -A unknown
      }
    }
  }

  return false; // Stop
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
  struct Email *e = NULL;
  SendFlags sendflags = SEND_NO_FLAGS;
  int rc = 1;
  bool repeat_error = false;
  struct Buffer *expanded_infile = buf_pool_get();
  struct Buffer *tempfile = buf_pool_get();
  struct ConfigSet *cs = NULL;
  struct CommandLine *cli = command_line_new();

  MuttLogger = log_disp_terminal;

  /* sanity check against stupid administrators */
  if (getegid() != getgid())
  {
    mutt_error("%s: I don't want to run with privileges!", (argc != 0) ? argv[0] : "neomutt");
    goto main_exit; // TEST01: neomutt (as root, chgrp mail neomutt; chmod +s neomutt)
  }

  OptGui = true;

  NeoMutt = neomutt_new();
  if (!neomutt_init(NeoMutt, envp, Modules))
    goto main_curses;

  cli_parse(argc, argv, cli);

  if (!show_help(&cli->help))
    goto main_ok;

  subjrx_init();
  attach_init();
  alternates_init();
  init_keys();

#ifdef USE_DEBUG_NOTIFY
  notify_observer_add(NeoMutt->notify, NT_ALL, debug_all_observer, NULL);
#endif

  cs = NeoMutt->sub->cs;
  if (!get_user_info(cs))
    goto main_exit;

  if (!init_logging(&cli->shared, cs))
    goto main_exit;

  mutt_log_prep();
  MuttLogger = log_disp_queue;
  log_translation();

  /* Check for a batch send. */
  if (!isatty(STDIN_FILENO) || !ARRAY_EMPTY(&cli->info.queries) ||
      !ARRAY_EMPTY(&cli->info.alias_queries) || cli->info.dump_config)
  {
    OptGui = false;
    sendflags |= SEND_BATCH;
    MuttLogger = log_disp_terminal;
    log_queue_flush(log_disp_terminal);
  }

  /* Check to make sure stdout is available in curses mode. */
  if (OptGui && !isatty(STDOUT_FILENO))
    goto main_curses;

  /* This must come before mutt_init() because curses needs to be started
   * before calling the init_pair() function to set the color scheme.  */
  if (OptGui)
  {
    int crc = start_curses();
    if (crc != 0)
      goto main_curses; // TEST08: can't test -- fake term?
  }

  /* Always create the mutt_windows because batch mode has some shared code
   * paths that end up referencing them. */
  rootwin_new();

  if (OptGui)
  {
    /* check whether terminal status is supported (must follow curses init) */
    TsSupported = mutt_ts_capability();
    mutt_resize_screen();
    log_gui();
  }

  alias_init();
  driver_tags_init();
  menu_init();
  sb_init();

  /* set defaults and read init files */
  int rc2 = mutt_init(cs, &cli->shared.log_level, &cli->shared.log_file,
                      cli->shared.disable_system, &cli->shared.user_files,
                      &cli->shared.commands);
  if (rc2 != 0)
    goto main_curses;

  mutt_hist_init();
  mutt_hist_read_file();

#ifdef USE_NOTMUCH
  const bool c_virtual_spool_file = cs_subset_bool(NeoMutt->sub, "virtual_spool_file");
  if (c_virtual_spool_file)
  {
    /* Find the first virtual folder and open it */
    struct MailboxArray ma = neomutt_mailboxes_get(NeoMutt, MUTT_NOTMUCH);
    struct Mailbox **mp = ARRAY_FIRST(&ma);
    if (mp)
      cs_str_string_set(cs, "spool_file", mailbox_path(*mp), NULL);
    ARRAY_FREE(&ma); // Clean up the ARRAY, but not the Mailboxes
  }
#endif

  km_set_abort_key();

  init_nntp(&cli->tui.nntp_server, cs);

  /* Initialize crypto backends.  */
  crypt_init();

  if (!buf_is_empty(&cli->shared.mbox_type) &&
      !config_str_set_initial(cs, "mbox_type", buf_string(&cli->shared.mbox_type)))
  {
    goto main_curses;
  }

  if (!dump_info(&cli->info, cs))
    goto main_ok;

  if (OptGui)
  {
    mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
    clear();
    MuttLogger = log_disp_curses;
    log_queue_flush(log_disp_curses);
    log_queue_set_max_size(100);
  }

#ifdef USE_AUTOCRYPT
  /* Initialize autocrypt after curses messages are working,
   * because of the initial account setup screens. */
  const bool c_autocrypt = cs_subset_bool(NeoMutt->sub, "autocrypt");
  if (c_autocrypt)
    mutt_autocrypt_init(!(sendflags & SEND_BATCH));
#endif

  /* Create the `$folder` directory if it doesn't exist. */
  const char *const c_folder = cs_subset_string(NeoMutt->sub, "folder");
  if (OptGui && c_folder)
  {
    struct stat st = { 0 };
    struct Buffer *fpath = buf_pool_get();

    buf_strcpy(fpath, c_folder);
    buf_expand_path(fpath);
    bool skip = false;
    /* we're not connected yet - skip mail folder creation */
    skip |= (imap_path_probe(buf_string(fpath), NULL) == MUTT_IMAP);
    skip |= (pop_path_probe(buf_string(fpath), NULL) == MUTT_POP);
    skip |= (nntp_path_probe(buf_string(fpath), NULL) == MUTT_NNTP);
    if (!skip && (stat(buf_string(fpath), &st) == -1) && (errno == ENOENT))
    {
      char msg2[256] = { 0 };
      snprintf(msg2, sizeof(msg2), _("%s does not exist. Create it?"), c_folder);
      if (query_yesorno(msg2, MUTT_YES) == MUTT_YES)
      {
        if ((mkdir(buf_string(fpath), 0700) == -1) && (errno != EEXIST))
          mutt_error(_("Can't create %s: %s"), c_folder, strerror(errno)); // TEST21: neomutt -n -F /dev/null (and ~/Mail doesn't exist)
      }
    }
    buf_pool_release(&fpath);
  }

  StartupComplete = true;

  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, main_hist_observer, NULL);
  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, main_log_observer, NULL);
  notify_observer_add(NeoMutt->notify, NT_TIMEOUT, main_timeout_observer, NULL);

  if (cli->tui.start_postponed)
  {
    if (OptGui)
      mutt_flushinp();
    if (mutt_send_message(SEND_POSTPONED, NULL, NULL, NULL, NULL, NeoMutt->sub) == 0)
      rc = 0;
    // TEST23: neomutt -p (postponed message, cancel)
    // TEST24: neomutt -p (no postponed message)
    log_queue_empty();
    repeat_error = true;
    goto main_curses;
  }
  else if (cli->send.is_set)
  {
    FILE *fp_in = NULL;
    FILE *fp_out = NULL;
    const char *infile = NULL;
    char *bodytext = NULL;
    const char *bodyfile = NULL;
    int rv = 0;

    if (OptGui)
      mutt_flushinp();

    e = email_new();
    e->env = mutt_env_new();

    const char **cp = NULL;
    ARRAY_FOREACH(cp, &cli->send.bcc_list)
    {
      mutt_addrlist_parse(&e->env->bcc, *cp);
    }

    ARRAY_FOREACH(cp, &cli->send.cc_list)
    {
      mutt_addrlist_parse(&e->env->cc, *cp);
    }

    ARRAY_FOREACH(cp, &cli->send.addresses)
    {
      if (url_check_scheme(*cp) == U_MAILTO)
      {
        if (!mutt_parse_mailto(e->env, &bodytext, *cp))
        {
          mutt_error(_("Failed to parse mailto: link"));
          email_free(&e);
          goto main_curses; // TEST25: neomutt mailto:?
        }
      }
      else
      {
        mutt_addrlist_parse(&e->env->to, *cp);
      }
    }

    const bool c_auto_edit = cs_subset_bool(NeoMutt->sub, "auto_edit");
    if (buf_is_empty(&cli->send.draft_file) && c_auto_edit &&
        TAILQ_EMPTY(&e->env->to) && TAILQ_EMPTY(&e->env->cc))
    {
      mutt_error(_("No recipients specified"));
      email_free(&e);
      goto main_curses; // TEST26: neomutt -s test (with auto_edit=yes)
    }

    if (!buf_is_empty(&cli->send.subject))
    {
      /* prevent header injection */
      mutt_filter_commandline_header_value(cli->send.subject.data);
      mutt_env_set_subject(e->env, buf_string(&cli->send.subject));
    }

    if (!buf_is_empty(&cli->send.draft_file))
    {
      infile = buf_string(&cli->send.draft_file);
    }
    else if (!buf_is_empty(&cli->send.include_file))
    {
      infile = buf_string(&cli->send.include_file);
    }
    else
    {
      cli->send.edit_infile = false;
    }

    if (infile || bodytext)
    {
      /* Prepare fp_in and expanded_infile. */
      if (infile)
      {
        if (mutt_str_equal("-", infile))
        {
          if (cli->send.edit_infile)
          {
            mutt_error(_("Can't use -E flag with stdin"));
            email_free(&e);
            goto main_curses; // TEST27: neomutt -E -H -
          }
          fp_in = stdin;
        }
        else
        {
          buf_strcpy(expanded_infile, infile);
          buf_expand_path(expanded_infile);
          fp_in = mutt_file_fopen(buf_string(expanded_infile), "r");
          if (!fp_in)
          {
            mutt_perror("%s", buf_string(expanded_infile));
            email_free(&e);
            goto main_curses; // TEST28: neomutt -E -H missing
          }
        }
      }

      if (cli->send.edit_infile)
      {
        /* If editing the infile, keep it around afterwards so
         * it doesn't get unlinked, and we can rebuild the draft_file */
        sendflags |= SEND_NO_FREE_HEADER;
      }
      else
      {
        /* Copy input to a tempfile, and re-point fp_in to the tempfile.
         * Note: stdin is always copied to a tempfile, ensuring draft_file
         * can stat and get the correct st_size below.  */
        buf_mktemp(tempfile);

        fp_out = mutt_file_fopen(buf_string(tempfile), "w");
        if (!fp_out)
        {
          mutt_file_fclose(&fp_in);
          mutt_perror("%s", buf_string(tempfile));
          email_free(&e);
          goto main_curses; // TEST29: neomutt -H existing-file (where tmpdir=/path/to/FILE blocking tmpdir)
        }
        if (fp_in)
        {
          mutt_file_copy_stream(fp_in, fp_out);
          if (fp_in == stdin)
            sendflags |= SEND_CONSUMED_STDIN;
          else
            mutt_file_fclose(&fp_in);
        }
        else if (bodytext)
        {
          fputs(bodytext, fp_out);
        }
        mutt_file_fclose(&fp_out);

        fp_in = mutt_file_fopen(buf_string(tempfile), "r");
        if (!fp_in)
        {
          mutt_perror("%s", buf_string(tempfile));
          email_free(&e);
          goto main_curses; // TEST30: can't test
        }
      }

      /* Parse the draft_file into the full Email/Body structure.
       * Set SEND_DRAFT_FILE so mutt_send_message doesn't overwrite
       * our e->body.  */
      if (!buf_is_empty(&cli->send.draft_file))
      {
        struct Envelope *opts_env = e->env;
        struct stat st = { 0 };

        sendflags |= SEND_DRAFT_FILE;

        /* Set up a tmp Email with just enough information so that
         * mutt_prepare_template() can parse the message in fp_in.  */
        struct Email *e_tmp = email_new();
        e_tmp->offset = 0;
        e_tmp->body = mutt_body_new();
        if (fstat(fileno(fp_in), &st) != 0)
        {
          mutt_perror("%s", buf_string(&cli->send.draft_file));
          email_free(&e);
          email_free(&e_tmp);
          goto main_curses; // TEST31: can't test
        }
        e_tmp->body->length = st.st_size;

        if (mutt_prepare_template(fp_in, NULL, e, e_tmp, false) < 0)
        {
          mutt_error(_("Can't parse message template: %s"),
                     buf_string(&cli->send.draft_file));
          email_free(&e);
          email_free(&e_tmp);
          goto main_curses;
        }

        /* Scan for neomutt header to set `$resume_draft_files` */
        struct ListNode *tmp = NULL;
        const bool c_resume_edited_draft_files = cs_subset_bool(NeoMutt->sub, "resume_edited_draft_files");
        struct ListNode *np = NULL;
        STAILQ_FOREACH_SAFE(np, &e->env->userhdrs, entries, tmp)
        {
          if (mutt_istr_startswith(np->data, "X-Mutt-Resume-Draft:"))
          {
            if (c_resume_edited_draft_files)
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
          mutt_env_set_subject(e->env, opts_env->subject);

        mutt_env_free(&opts_env);
        email_free(&e_tmp);
      }
      else if (cli->send.edit_infile)
      {
        /* Editing the include_file: pass it directly in.
         * Note that SEND_NO_FREE_HEADER is set above so it isn't unlinked.  */
        bodyfile = buf_string(expanded_infile);
      }
      else
      {
        // For bodytext and unedited include_file: use the tempfile.
        bodyfile = buf_string(tempfile);
      }

      mutt_file_fclose(&fp_in);
    }

    FREE(&bodytext);

    if (!ARRAY_EMPTY(&cli->send.attach))
    {
      struct Body *b = e->body;

      while (b && b->next)
        b = b->next;

      ARRAY_FOREACH(cp, &cli->send.attach)
      {
        if (b)
        {
          b->next = mutt_make_file_attach(*cp, NeoMutt->sub);
          b = b->next;
        }
        else
        {
          b = mutt_make_file_attach(*cp, NeoMutt->sub);
          e->body = b;
        }
        if (!b)
        {
          mutt_error(_("%s: unable to attach file"), *cp);
          email_free(&e);
          goto main_curses; // TEST32: neomutt john@example.com -a missing
        }
      }
    }

    rv = mutt_send_message(sendflags, e, bodyfile, NULL, NULL, NeoMutt->sub);
    /* We WANT the "Mail sent." and any possible, later error */
    log_queue_empty();
    if (ErrorBufMessage)
      mutt_message("%s", ErrorBuf);

    if (cli->send.edit_infile)
    {
      if (!buf_is_empty(&cli->send.draft_file))
      {
        if (truncate(buf_string(expanded_infile), 0) == -1)
        {
          mutt_perror("%s", buf_string(expanded_infile));
          email_free(&e);
          goto main_curses; // TEST33: neomutt -H read-only -s test john@example.com -E
        }
        fp_out = mutt_file_fopen(buf_string(expanded_infile), "a");
        if (!fp_out)
        {
          mutt_perror("%s", buf_string(expanded_infile));
          email_free(&e);
          goto main_curses; // TEST34: can't test
        }

        /* If the message was sent or postponed, these will already
         * have been done.  */
        if (rv < 0)
        {
          if (e->body->next)
            e->body = mutt_make_multipart(e->body);
          mutt_encode_descriptions(e->body, true, NeoMutt->sub);
          mutt_prepare_envelope(e->env, false, NeoMutt->sub);
          mutt_env_to_intl(e->env, NULL, NULL);
        }

        const bool c_crypt_protected_headers_read = cs_subset_bool(NeoMutt->sub, "crypt_protected_headers_read");
        mutt_rfc822_write_header(fp_out, e->env, e->body, MUTT_WRITE_HEADER_POSTPONE, false,
                                 c_crypt_protected_headers_read &&
                                     mutt_should_hide_protected_subject(e),
                                 NeoMutt->sub);
        const bool c_resume_edited_draft_files = cs_subset_bool(NeoMutt->sub, "resume_edited_draft_files");
        if (c_resume_edited_draft_files)
          fprintf(fp_out, "X-Mutt-Resume-Draft: 1\n");
        fputc('\n', fp_out);
        if ((mutt_write_mime_body(e->body, fp_out, NeoMutt->sub) == -1))
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
    if (!buf_is_empty(tempfile))
      unlink(buf_string(tempfile));

    rootwin_cleanup();

    if (rv != 0)
      goto main_curses; // TEST36: neomutt -H existing -s test john@example.com -E (cancel sending)
  }
  else if (sendflags & SEND_BATCH)
  {
    /* This guards against invoking `neomutt < /dev/null` and accidentally
     * sending an email due to a my-header or other setting.  */
    mutt_error(_("No recipients specified"));
    goto main_curses;
  }
  else
  {
    struct Buffer *folder = &cli->tui.folder;
    bool explicit_folder = !buf_is_empty(folder);

    if (cli->tui.start_new_mail)
    {
      const bool c_imap_passive = cs_subset_bool(NeoMutt->sub, "imap_passive");
      cs_subset_str_native_set(NeoMutt->sub, "imap_passive", false, NULL);
      const CheckStatsFlags csflags = MUTT_MAILBOX_CHECK_IMMEDIATE;
      if (mutt_mailbox_check(NULL, csflags) == 0)
      {
        mutt_message(_("No mailbox with new mail"));
        repeat_error = true;
        goto main_curses; // TEST37: neomutt -Z (no new mail)
      }
      buf_reset(folder);
      mutt_mailbox_next(NULL, folder);
      cs_subset_str_native_set(NeoMutt->sub, "imap_passive", c_imap_passive, NULL);
    }
    else if (cli->tui.start_nntp || cli->tui.start_browser)
    {
      if (cli->tui.start_nntp)
      {
        const char *const c_news_server = cs_subset_string(NeoMutt->sub, "news_server");
        OptNews = true;
        CurrentNewsSrv = nntp_select_server(NULL, c_news_server, false);
        if (!CurrentNewsSrv)
          goto main_curses; // TEST38: neomutt -G (unset news_server)
      }
      else if (ARRAY_EMPTY(&NeoMutt->accounts))
      {
        mutt_error(_("No incoming mailboxes defined"));
        goto main_curses; // TEST39: neomutt -n -F /dev/null -y
      }
      buf_reset(folder);
      dlg_browser(folder, MUTT_SEL_FOLDER | MUTT_SEL_MAILBOX, NULL, NULL, NULL);
      if (buf_is_empty(folder))
      {
        goto main_ok; // TEST40: neomutt -y (quit selection)
      }
    }

    if (buf_is_empty(folder))
    {
      const char *const c_spool_file = cs_subset_string(NeoMutt->sub, "spool_file");
      if (c_spool_file)
      {
        // Check if `$spool_file` corresponds a mailboxes' description.
        struct Mailbox *m_desc = mailbox_find_name(c_spool_file);
        if (m_desc)
          buf_strcpy(folder, m_desc->realpath);
        else
          buf_strcpy(folder, c_spool_file);
      }
      else if (c_folder)
      {
        buf_strcpy(folder, c_folder);
      }
      /* else no folder */
    }

    if (OptNews)
    {
      OptNews = false;
      buf_alloc(folder, PATH_MAX);
      nntp_expand_path(folder->data, folder->dsize, &CurrentNewsSrv->conn->account);
    }
    else
    {
      buf_expand_path(folder);
    }

    mutt_str_replace(&CurrentFolder, buf_string(folder));
    mutt_str_replace(&LastFolder, buf_string(folder));

    if (cli->tui.start_any_mail || cli->tui.start_new_mail)
    {
      /* check to see if there are any messages in the folder */
      switch (mx_path_is_empty(folder))
      {
        case -1:
          mutt_perror("%s", buf_string(folder));
          goto main_curses; // TEST41: neomutt -z -f missing
        case 1:
          mutt_error(_("Mailbox is empty"));
          goto main_curses; // TEST42: neomutt -z -f /dev/null
      }
    }

    struct Mailbox *m_cur = mailbox_find(buf_string(folder));
    // Take a copy of the name just in case the hook alters m_cur
    const char *name = m_cur ? mutt_str_dup(m_cur->name) : NULL;
    mutt_folder_hook(buf_string(folder), name);
    FREE(&name);
    mutt_startup_shutdown_hook(CMD_STARTUP_HOOK);
    mutt_debug(LL_NOTIFY, "NT_GLOBAL_STARTUP\n");
    notify_send(NeoMutt->notify, NT_GLOBAL, NT_GLOBAL_STARTUP, NULL);

    notify_send(NeoMutt->notify_resize, NT_RESIZE, 0, NULL);
    window_redraw(NULL);

    repeat_error = true;
    struct Mailbox *m = mx_resolve(buf_string(folder));
    const bool c_read_only = cs_subset_bool(NeoMutt->sub, "read_only");
    if (!mx_mbox_open(m, (cli->tui.read_only || c_read_only) ? MUTT_READONLY : MUTT_OPEN_NO_FLAGS))
    {
      if (m->account)
        account_mailbox_remove(m->account, m);

      mailbox_free(&m);
      mutt_error(_("Unable to open mailbox %s"), buf_string(folder));
      repeat_error = false;
    }
    if (m || !explicit_folder)
    {
      struct MuttWindow *dlg = index_pager_init();
      dialog_push(dlg);

      mutt_curses_set_cursor(MUTT_CURSOR_INVISIBLE);
      m = dlg_index(dlg, m);
      mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);
      mailbox_free(&m);

      dialog_pop();
      mutt_window_free(&dlg);
      log_queue_empty();
      repeat_error = false;
    }
    imap_logout_all();
#ifdef USE_SASL_CYRUS
    mutt_sasl_cleanup();
#endif
#ifdef USE_SASL_GNU
    mutt_gsasl_cleanup();
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
  mutt_temp_attachments_cleanup();
  /* Repeat the last message to the user */
  if (repeat_error && ErrorBufMessage)
    puts(ErrorBuf);
main_exit:
  if (NeoMutt && NeoMutt->sub)
  {
    notify_observer_remove(NeoMutt->sub->notify, main_hist_observer, NULL);
    notify_observer_remove(NeoMutt->sub->notify, main_log_observer, NULL);
    notify_observer_remove(NeoMutt->notify, main_timeout_observer, NULL);
  }
  MuttLogger = log_disp_queue;
  buf_pool_release(&expanded_infile);
  buf_pool_release(&tempfile);
  crypto_module_cleanup();
  rootwin_cleanup();
  buf_pool_cleanup();
  if (NeoMutt)
    envlist_free(&NeoMutt->env);
  mutt_browser_cleanup();
  external_cleanup();
  menu_cleanup();
  crypt_cleanup();
  mutt_ch_cache_cleanup();
  command_line_free(&cli);

  source_stack_cleanup();

  alias_cleanup();
  sb_cleanup();

  mutt_regexlist_free(&MailLists);
  mutt_regexlist_free(&NoSpamList);
  mutt_regexlist_free(&SubscribedLists);
  mutt_regexlist_free(&UnMailLists);
  mutt_regexlist_free(&UnSubscribedLists);

  driver_tags_cleanup();

  /* Lists of strings */
  mutt_list_free(&AlternativeOrderList);
  mutt_list_free(&AutoViewList);
  mutt_list_free(&HeaderOrderList);
  mutt_list_free(&Ignore);
  mutt_list_free(&MailToAllow);
  mutt_list_free(&MimeLookupList);
  mutt_list_free(&UnIgnore);
  mutt_list_free(&UserHeader);

  colors_cleanup();

  FREE(&CurrentFolder);
  FREE(&LastFolder);
  FREE(&ShortHostname);

  mutt_replacelist_free(&SpamList);

  mutt_delete_hooks(CMD_NONE);

  mutt_hist_cleanup();

  mutt_regexlist_free(&NoSpamList);
  if (NeoMutt)
    commands_clear(&NeoMutt->commands);

  lua_cleanup();
  subjrx_cleanup();
  attach_cleanup();
  alternates_cleanup();
  km_cleanup();
  mutt_prex_cleanup();
  config_cache_cleanup();
  neomutt_cleanup(NeoMutt);
  neomutt_free(&NeoMutt);
  log_queue_flush(log_disp_terminal);
  mutt_log_stop();
  return rc;
}
