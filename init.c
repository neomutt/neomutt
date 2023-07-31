/**
 * @file
 * Config/command parsing
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2013,2016 Michael R. Elkins <me@mutt.org>
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
 * @page neo_init Config/command parsing
 *
 * Config/command parsing
 */

#include "config.h"
#include <errno.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "init.h"
#include "color/lib.h"
#include "history/lib.h"
#include "notmuch/lib.h"
#include "parse/lib.h"
#include "commands.h"
#include "globals.h" // IWYU pragma: keep
#include "hook.h"
#include "keymap.h"
#ifdef USE_LUA
#include "mutt_lua.h"
#endif
#include "menu/lib.h"
#include "muttlib.h"
#include "protos.h"
#ifdef USE_SIDEBAR
#include "sidebar/lib.h"
#endif
#ifdef USE_COMP_MBOX
#include "compmbox/lib.h"
#endif
#ifdef USE_IMAP
#include "imap/lib.h"
#endif

/**
 * execute_commands - Execute a set of NeoMutt commands
 * @param p List of command strings
 * @retval  0 Success, all the commands succeeded
 * @retval -1 Error
 */
static int execute_commands(struct ListHead *p)
{
  int rc = 0;
  struct Buffer *err = buf_pool_get();

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, p, entries)
  {
    enum CommandResult rc2 = parse_rc_line(np->data, err);
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

  for (int i = 0; locations[i][0] || locations[i][1]; i++)
  {
    if (!locations[i][0])
      continue;

    for (int j = 0; names[j]; j++)
    {
      char buf[256] = { 0 };

      snprintf(buf, sizeof(buf), "%s/%s%s", locations[i][0], locations[i][1], names[j]);
      if (access(buf, F_OK) == 0)
        return mutt_str_dup(buf);
    }
  }

  return NULL;
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

  for (size_t i = 0; i < mutt_array_size(mn_files); i++)
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
    cs_str_initial_set(cs, "hostname", fqdn, NULL);
    cs_str_reset(cs, "hostname", NULL);
    FREE(&fqdn);
  }

  return true;
}

/**
 * mutt_opts_cleanup - Clean up before quitting
 */
void mutt_opts_cleanup(void)
{
  source_stack_cleanup();

  alias_cleanup();
#ifdef USE_SIDEBAR
  sb_cleanup();
#endif

  mutt_regexlist_free(&MailLists);
  mutt_regexlist_free(&NoSpamList);
  mutt_regexlist_free(&SubscribedLists);
  mutt_regexlist_free(&UnMailLists);
  mutt_regexlist_free(&UnSubscribedLists);

  mutt_grouplist_cleanup();
  driver_tags_cleanup();

  /* Lists of strings */
  mutt_list_free(&AlternativeOrderList);
  mutt_list_free(&AutoViewList);
  mutt_list_free(&HeaderOrderList);
  mutt_list_free(&Ignore);
  mutt_list_free(&MailToAllow);
  mutt_list_free(&MimeLookupList);
  mutt_list_free(&Muttrc);
  mutt_list_free(&UnIgnore);
  mutt_list_free(&UserHeader);

  mutt_colors_cleanup();

  FREE(&CurrentFolder);
  FREE(&HomeDir);
  FREE(&LastFolder);
  FREE(&ShortHostname);
  FREE(&Username);

  mutt_replacelist_free(&SpamList);

  mutt_delete_hooks(MUTT_HOOK_NO_FLAGS);

  mutt_hist_cleanup();
  mutt_keys_cleanup();

  mutt_regexlist_free(&NoSpamList);
  commands_cleanup();
}

/**
 * mutt_init - Initialise NeoMutt
 * @param cs          Config Set
 * @param skip_sys_rc If true, don't read the system config file
 * @param commands    List of config commands to execute
 * @retval 0 Success
 * @retval 1 Error
 */
int mutt_init(struct ConfigSet *cs, bool skip_sys_rc, struct ListHead *commands)
{
  int need_pause = 0;
  int rc = 1;
  struct Buffer err = buf_make(256);
  struct Buffer buf = buf_make(256);

  mutt_grouplist_init();
  alias_init();
  commands_init();
  hooks_init();
#ifdef USE_COMP_MBOX
  mutt_comp_init();
#endif
#ifdef USE_IMAP
  imap_init();
#endif
#ifdef USE_LUA
  mutt_lua_init();
#endif
  driver_tags_init();

  menu_init();
#ifdef USE_SIDEBAR
  sb_init();
#endif
#ifdef USE_NOTMUCH
  nm_init();
#endif

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
      cs_subset_str_native_set(NeoMutt->sub, "color_directcolor", true, NULL);
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
    buf_concat_path(&buf, NONULL(HomeDir), MAILPATH);
#else
    buf_concat_path(&buf, MAILPATH, NONULL(Username));
#endif
    p = buf_string(&buf);
  }
  cs_str_initial_set(cs, "spool_file", p, NULL);
  cs_str_reset(cs, "spool_file", NULL);

  p = mutt_str_getenv("REPLYTO");
  if (p)
  {
    struct Buffer token;

    buf_printf(&buf, "Reply-To: %s", p);
    buf_init(&token);
    parse_my_hdr(&token, &buf, 0, &err); /* adds to UserHeader */
    FREE(&token.data);
  }

  p = mutt_str_getenv("EMAIL");
  if (p)
  {
    cs_str_initial_set(cs, "from", p, NULL);
    cs_str_reset(cs, "from", NULL);
  }

  /* "$mailcap_path" precedence: config file, environment, code */
  const char *env_mc = mutt_str_getenv("MAILCAPS");
  if (env_mc)
  {
    cs_str_initial_set(cs, "mailcap_path", env_mc, NULL);
    cs_str_reset(cs, "mailcap_path", NULL);
  }

  /* "$tmp_dir" precedence: config file, environment, code */
  const char *env_tmp = mutt_str_getenv("TMPDIR");
  if (env_tmp)
  {
    cs_str_initial_set(cs, "tmp_dir", env_tmp, NULL);
    cs_str_reset(cs, "tmp_dir", NULL);
  }

  /* "$visual", "$editor" precedence: config file, environment, code */
  const char *env_ed = mutt_str_getenv("VISUAL");
  if (!env_ed)
    env_ed = mutt_str_getenv("EDITOR");
  if (!env_ed)
    env_ed = "vi";
  cs_str_initial_set(cs, "editor", env_ed, NULL);

  const char *const c_editor = cs_subset_string(NeoMutt->sub, "editor");
  if (!c_editor)
    cs_str_reset(cs, "editor", NULL);

  const char *charset = mutt_ch_get_langinfo_charset();
  cs_str_initial_set(cs, "charset", charset, NULL);
  cs_str_reset(cs, "charset", NULL);
  mutt_ch_set_charset(charset);
  FREE(&charset);

#ifdef HAVE_GETSID
  /* Unset suspend by default if we're the session leader */
  if (getsid(0) == getpid())
  {
    cs_str_initial_set(cs, "suspend", "no", NULL);
    cs_str_reset(cs, "suspend", NULL);
  }
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

  if (STAILQ_EMPTY(&Muttrc))
  {
    const char *xdg_cfg_home = mutt_str_getenv("XDG_CONFIG_HOME");

    if (!xdg_cfg_home && HomeDir)
    {
      buf_printf(&buf, "%s/.config", HomeDir);
      xdg_cfg_home = buf_string(&buf);
    }

    char *config = find_cfg(HomeDir, xdg_cfg_home);
    if (config)
    {
      mutt_list_insert_tail(&Muttrc, config);
    }
  }
  else
  {
    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &Muttrc, entries)
    {
      buf_strcpy(&buf, np->data);
      FREE(&np->data);
      buf_expand_path(&buf);
      np->data = buf_strdup(&buf);
      if (access(np->data, F_OK))
      {
        mutt_perror("%s", np->data);
        goto done; // TEST10: neomutt -F missing
      }
    }
  }

  if (!STAILQ_EMPTY(&Muttrc))
  {
    cs_str_string_set(cs, "alias_file", STAILQ_FIRST(&Muttrc)->data, NULL);
  }

  /* Process the global rc file if it exists and the user hasn't explicitly
   * requested not to via "-n".  */
  if (!skip_sys_rc)
  {
    do
    {
      if (mutt_set_xdg_path(XDG_CONFIG_DIRS, &buf))
        break;

      buf_printf(&buf, "%s/neomuttrc", SYSCONFDIR);
      if (access(buf_string(&buf), F_OK) == 0)
        break;

      buf_printf(&buf, "%s/Muttrc", SYSCONFDIR);
      if (access(buf_string(&buf), F_OK) == 0)
        break;

      buf_printf(&buf, "%s/neomuttrc", PKGDATADIR);
      if (access(buf_string(&buf), F_OK) == 0)
        break;

      buf_printf(&buf, "%s/Muttrc", PKGDATADIR);
    } while (false);

    if (access(buf_string(&buf), F_OK) == 0)
    {
      if (source_rc(buf_string(&buf), &err) != 0)
      {
        mutt_error("%s", err.data);
        need_pause = 1; // TEST11: neomutt (error in /etc/neomuttrc)
      }
    }
  }

  /* Read the user's initialization file.  */
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &Muttrc, entries)
  {
    if (np->data)
    {
      if (source_rc(np->data, &err) != 0)
      {
        mutt_error("%s", err.data);
        need_pause = 1; // TEST12: neomutt (error in ~/.neomuttrc)
      }
    }
  }

  if (execute_commands(commands) != 0)
    need_pause = 1; // TEST13: neomutt -e broken

  if (!get_hostname(cs))
    goto done;

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
  cs_str_initial_set(cs, "real_name", c_real_name, NULL);
  cs_str_reset(cs, "real_name", NULL);

  if (need_pause && !OptNoCurses)
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

  mutt_hist_init();
  mutt_hist_read_file();

#ifdef USE_NOTMUCH
  const bool c_virtual_spool_file = cs_subset_bool(NeoMutt->sub, "virtual_spool_file");
  if (c_virtual_spool_file)
  {
    /* Find the first virtual folder and open it */
    struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
    neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_NOTMUCH);
    struct MailboxNode *mp = STAILQ_FIRST(&ml);
    if (mp)
      cs_str_string_set(cs, "spool_file", mailbox_path(mp->mailbox), NULL);
    neomutt_mailboxlist_clear(&ml);
  }
#endif
  rc = 0;

done:
  buf_dealloc(&err);
  buf_dealloc(&buf);
  return rc;
}

/**
 * mutt_query_variables - Implement the -Q command line flag
 * @param queries   List of query strings
 * @param show_docs If true, show one-liner docs for the config item
 * @retval 0 Success, all queries exist
 * @retval 1 Error
 */
int mutt_query_variables(struct ListHead *queries, bool show_docs)
{
  struct Buffer value = buf_make(256);
  struct Buffer tmp = buf_make(256);
  int rc = 0;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, queries, entries)
  {
    buf_reset(&value);

    struct HashElem *he = cs_subset_lookup(NeoMutt->sub, np->data);
    if (he)
    {
      if (he->type & DT_DEPRECATED)
      {
        mutt_warning(_("Config variable '%s' is deprecated"), np->data);
        rc = 1;
        continue;
      }

      int rv = cs_subset_he_string_get(NeoMutt->sub, he, &value);
      if (CSR_RESULT(rv) != CSR_SUCCESS)
      {
        rc = 1;
        continue;
      }

      int type = DTYPE(he->type);
      if (type == DT_PATH)
        mutt_pretty_mailbox(value.data, value.dsize);

      if ((type != DT_BOOL) && (type != DT_NUMBER) && (type != DT_LONG) && (type != DT_QUAD))
      {
        buf_reset(&tmp);
        pretty_var(value.data, &tmp);
        buf_strcpy(&value, tmp.data);
      }

      dump_config_neo(NeoMutt->sub->cs, he, &value, NULL,
                      show_docs ? CS_DUMP_SHOW_DOCS : CS_DUMP_NO_FLAGS, stdout);
      continue;
    }

    mutt_warning(_("No such variable: %s"), np->data);
    rc = 1;
  }

  buf_dealloc(&value);
  buf_dealloc(&tmp);

  return rc; // TEST16: neomutt -Q charset
}
