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
#include <ctype.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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
#include "conn/lib.h" // IWYU pragma: keep
#include "gui/lib.h"
#include "mutt.h"
#include "init.h"
#include "history/lib.h"
#include "notmuch/lib.h"
#include "command_parse.h"
#include "context.h"
#include "functions.h"
#include "hook.h"
#include "keymap.h"
#include "mutt_commands.h"
#include "mutt_globals.h"
#ifdef USE_LUA
#include "mutt_lua.h"
#endif
#include "menu/lib.h"
#include "muttlib.h"
#include "myvar.h"
#include "options.h"
#include "protos.h"
#include "sort.h"
#ifdef USE_SIDEBAR
#include "sidebar/lib.h"
#endif
#ifdef USE_COMP_MBOX
#include "compmbox/lib.h"
#endif
#ifdef USE_IMAP
#include "imap/lib.h"
#endif

/* Initial string that starts completion. No telling how much the user has
 * typed so far. Allocate 1024 just to be sure! */
static char UserTyped[1024] = { 0 };

static int NumMatched = 0;          /* Number of matches for completion */
static char Completed[256] = { 0 }; /* completed string (command or variable) */
static const char **Matches;
/* this is a lie until mutt_init runs: */
static int MatchesListsize = 512; // Enough space for all of the config items

#ifdef USE_NOTMUCH
/* List of tags found in last call to mutt_nm_query_complete(). */
static char **nm_tags;
#endif

/**
 * matches_ensure_morespace - Allocate more space for auto-completion
 * @param current Current allocation
 */
static void matches_ensure_morespace(int current)
{
  if (current <= (MatchesListsize - 2))
    return;

  int base_space = 512; // Enough space for all of the config items
  int extra_space = MatchesListsize - base_space;
  extra_space *= 2;
  const int space = base_space + extra_space;
  mutt_mem_realloc(&Matches, space * sizeof(char *));
  memset(&Matches[current + 1], 0, space - current);
  MatchesListsize = space;
}

/**
 * candidate - helper function for completion
 * @param user User entered data for completion
 * @param src  Candidate for completion
 * @param dest Completion result gets here
 * @param dlen Length of dest buffer
 *
 * Changes the dest buffer if necessary/possible to aid completion.
 */
static void candidate(char *user, const char *src, char *dest, size_t dlen)
{
  if (!dest || !user || !src)
    return;

  if (strstr(src, user) != src)
    return;

  matches_ensure_morespace(NumMatched);
  Matches[NumMatched++] = src;
  if (dest[0] == '\0')
    mutt_str_copy(dest, src, dlen);
  else
  {
    int l;
    for (l = 0; src[l] && src[l] == dest[l]; l++)
      ; // do nothing

    dest[l] = '\0';
  }
}

#ifdef USE_NOTMUCH
/**
 * complete_all_nm_tags - Pass a list of Notmuch tags to the completion code
 * @param pt List of all Notmuch tags
 * @retval  0 Success
 * @retval -1 Error
 */
static int complete_all_nm_tags(const char *pt)
{
  struct Mailbox *m = ctx_mailbox(Context);
  int tag_count_1 = 0;
  int tag_count_2 = 0;

  NumMatched = 0;
  mutt_str_copy(UserTyped, pt, sizeof(UserTyped));
  memset(Matches, 0, MatchesListsize);
  memset(Completed, 0, sizeof(Completed));

  nm_db_longrun_init(m, false);

  /* Work out how many tags there are. */
  if (nm_get_all_tags(m, NULL, &tag_count_1) || (tag_count_1 == 0))
    goto done;

  /* Free the old list, if any. */
  if (nm_tags)
  {
    for (int i = 0; nm_tags[i]; i++)
      FREE(&nm_tags[i]);
    FREE(&nm_tags);
  }
  /* Allocate a new list, with sentinel. */
  nm_tags = mutt_mem_malloc((tag_count_1 + 1) * sizeof(char *));
  nm_tags[tag_count_1] = NULL;

  /* Get all the tags. */
  if (nm_get_all_tags(m, nm_tags, &tag_count_2) || (tag_count_1 != tag_count_2))
  {
    FREE(&nm_tags);
    nm_tags = NULL;
    nm_db_longrun_done(m);
    return -1;
  }

  /* Put them into the completion machinery. */
  for (int num = 0; num < tag_count_1; num++)
  {
    candidate(UserTyped, nm_tags[num], Completed, sizeof(Completed));
  }

  matches_ensure_morespace(NumMatched);
  Matches[NumMatched++] = UserTyped;

done:
  nm_db_longrun_done(m);
  return 0;
}
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
  struct Buffer *err = mutt_buffer_pool_get();

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, p, entries)
  {
    enum CommandResult rc2 = mutt_parse_rc_line(np->data, err);
    if (rc2 == MUTT_CMD_ERROR)
      mutt_error(_("Error in command line: %s"), mutt_buffer_string(err));
    else if (rc2 == MUTT_CMD_WARNING)
      mutt_warning(_("Warning in command line: %s"), mutt_buffer_string(err));

    if ((rc2 == MUTT_CMD_ERROR) || (rc2 == MUTT_CMD_WARNING))
    {
      mutt_buffer_pool_release(&err);
      return -1;
    }
  }
  mutt_buffer_pool_release(&err);

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
      char buf[256];

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
 * @retval true  Success
 * @retval false Error, failed to find any name
 *
 * Use several methods to try to find the Fully-Qualified domain name of this host.
 * If the user has already configured a hostname, this function will use it.
 */
static bool get_hostname(struct ConfigSet *cs)
{
  const char *short_host = NULL;
  struct utsname utsname;

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
    /* now get FQDN.  Use configured domain first, DNS next, then uname */
#ifdef DOMAIN
    /* we have a compile-time domain name, use that for `$hostname` */
    fqdn = mutt_mem_malloc(mutt_str_len(DOMAIN) + mutt_str_len(ShortHostname) + 2);
    sprintf((char *) fqdn, "%s.%s", NONULL(ShortHostname), DOMAIN);
#else
    fqdn = getmailname();
    if (!fqdn)
    {
      struct Buffer *domain = mutt_buffer_pool_get();
      if (getdnsdomainname(domain) == 0)
      {
        fqdn = mutt_mem_malloc(mutt_buffer_len(domain) + mutt_str_len(ShortHostname) + 2);
        sprintf((char *) fqdn, "%s.%s", NONULL(ShortHostname), mutt_buffer_string(domain));
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
      mutt_buffer_pool_release(&domain);
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
 * mutt_extract_token - Extract one token from a string
 * @param dest  Buffer for the result
 * @param tok   Buffer containing tokens
 * @param flags Flags, see #TokenFlags
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_extract_token(struct Buffer *dest, struct Buffer *tok, TokenFlags flags)
{
  if (!dest || !tok)
    return -1;

  char ch;
  char qc = '\0'; /* quote char */
  char *pc = NULL;

  /* Some callers used to rely on the (bad) assumption that dest->data would be
   * non-NULL after calling this function.  Perhaps I've missed a few cases, or
   * a future caller might make the same mistake.  */
  if (!dest->data)
    mutt_buffer_alloc(dest, 256);

  mutt_buffer_reset(dest);

  SKIPWS(tok->dptr);
  while ((ch = *tok->dptr))
  {
    if (qc == '\0')
    {
      if ((IS_SPACE(ch) && !(flags & MUTT_TOKEN_SPACE)) ||
          ((ch == '#') && !(flags & MUTT_TOKEN_COMMENT)) ||
          ((ch == '+') && (flags & MUTT_TOKEN_PLUS)) ||
          ((ch == '-') && (flags & MUTT_TOKEN_MINUS)) ||
          ((ch == '=') && (flags & MUTT_TOKEN_EQUAL)) ||
          ((ch == '?') && (flags & MUTT_TOKEN_QUESTION)) ||
          ((ch == ';') && !(flags & MUTT_TOKEN_SEMICOLON)) ||
          ((flags & MUTT_TOKEN_PATTERN) && strchr("~%=!|", ch)))
      {
        break;
      }
    }

    tok->dptr++;

    if (ch == qc)
      qc = 0; /* end of quote */
    else if (!qc && ((ch == '\'') || (ch == '"')) && !(flags & MUTT_TOKEN_QUOTE))
      qc = ch;
    else if ((ch == '\\') && (qc != '\''))
    {
      if (tok->dptr[0] == '\0')
        return -1; /* premature end of token */
      switch (ch = *tok->dptr++)
      {
        case 'c':
        case 'C':
          if (tok->dptr[0] == '\0')
            return -1; /* premature end of token */
          mutt_buffer_addch(dest, (toupper((unsigned char) tok->dptr[0]) - '@') & 0x7f);
          tok->dptr++;
          break;
        case 'e':
          mutt_buffer_addch(dest, '\033'); // Escape
          break;
        case 'f':
          mutt_buffer_addch(dest, '\f');
          break;
        case 'n':
          mutt_buffer_addch(dest, '\n');
          break;
        case 'r':
          mutt_buffer_addch(dest, '\r');
          break;
        case 't':
          mutt_buffer_addch(dest, '\t');
          break;
        default:
          if (isdigit((unsigned char) ch) && isdigit((unsigned char) tok->dptr[0]) &&
              isdigit((unsigned char) tok->dptr[1]))
          {
            mutt_buffer_addch(dest, (ch << 6) + (tok->dptr[0] << 3) + tok->dptr[1] - 3504);
            tok->dptr += 2;
          }
          else
            mutt_buffer_addch(dest, ch);
      }
    }
    else if ((ch == '^') && (flags & MUTT_TOKEN_CONDENSE))
    {
      if (tok->dptr[0] == '\0')
        return -1; /* premature end of token */
      ch = *tok->dptr++;
      if (ch == '^')
        mutt_buffer_addch(dest, ch);
      else if (ch == '[')
        mutt_buffer_addch(dest, '\033'); // Escape
      else if (isalpha((unsigned char) ch))
        mutt_buffer_addch(dest, toupper((unsigned char) ch) - '@');
      else
      {
        mutt_buffer_addch(dest, '^');
        mutt_buffer_addch(dest, ch);
      }
    }
    else if ((ch == '`') && (!qc || (qc == '"')))
    {
      FILE *fp = NULL;
      pid_t pid;

      pc = tok->dptr;
      do
      {
        pc = strpbrk(pc, "\\`");
        if (pc)
        {
          /* skip any quoted chars */
          if (*pc == '\\')
            pc += 2;
        }
      } while (pc && (pc[0] != '`'));
      if (!pc)
      {
        mutt_debug(LL_DEBUG1, "mismatched backticks\n");
        return -1;
      }
      struct Buffer cmd;
      mutt_buffer_init(&cmd);
      *pc = '\0';
      if (flags & MUTT_TOKEN_BACKTICK_VARS)
      {
        /* recursively extract tokens to interpolate variables */
        mutt_extract_token(&cmd, tok,
                           MUTT_TOKEN_QUOTE | MUTT_TOKEN_SPACE | MUTT_TOKEN_COMMENT |
                               MUTT_TOKEN_SEMICOLON | MUTT_TOKEN_NOSHELL);
      }
      else
      {
        cmd.data = mutt_str_dup(tok->dptr);
      }
      *pc = '`';
      pid = filter_create(cmd.data, NULL, &fp, NULL);
      if (pid < 0)
      {
        mutt_debug(LL_DEBUG1, "unable to fork command: %s\n", cmd.data);
        FREE(&cmd.data);
        return -1;
      }
      FREE(&cmd.data);

      tok->dptr = pc + 1;

      /* read line */
      struct Buffer expn = mutt_buffer_make(0);
      expn.data = mutt_file_read_line(NULL, &expn.dsize, fp, NULL, MUTT_RL_NO_FLAGS);
      mutt_file_fclose(&fp);
      filter_wait(pid);

      /* if we got output, make a new string consisting of the shell output
       * plus whatever else was left on the original line */
      /* BUT: If this is inside a quoted string, directly add output to
       * the token */
      if (expn.data)
      {
        if (qc)
        {
          mutt_buffer_addstr(dest, expn.data);
        }
        else
        {
          struct Buffer *copy = mutt_buffer_pool_get();
          mutt_buffer_fix_dptr(&expn);
          mutt_buffer_copy(copy, &expn);
          mutt_buffer_addstr(copy, tok->dptr);
          mutt_buffer_copy(tok, copy);
          mutt_buffer_seek(tok, 0);
          mutt_buffer_pool_release(&copy);
        }
        FREE(&expn.data);
      }
    }
    else if ((ch == '$') && (!qc || (qc == '"')) &&
             ((tok->dptr[0] == '{') || isalpha((unsigned char) tok->dptr[0])))
    {
      const char *env = NULL;
      char *var = NULL;

      if (tok->dptr[0] == '{')
      {
        pc = strchr(tok->dptr, '}');
        if (pc)
        {
          var = mutt_strn_dup(tok->dptr + 1, pc - (tok->dptr + 1));
          tok->dptr = pc + 1;

          if ((flags & MUTT_TOKEN_NOSHELL))
          {
            mutt_buffer_addch(dest, ch);
            mutt_buffer_addch(dest, '{');
            mutt_buffer_addstr(dest, var);
            mutt_buffer_addch(dest, '}');
            FREE(&var);
          }
        }
      }
      else
      {
        for (pc = tok->dptr; isalnum((unsigned char) *pc) || (pc[0] == '_'); pc++)
          ; // do nothing

        var = mutt_strn_dup(tok->dptr, pc - tok->dptr);
        tok->dptr = pc;
      }
      if (var)
      {
        struct Buffer result;
        mutt_buffer_init(&result);
        int rc = cs_subset_str_string_get(NeoMutt->sub, var, &result);

        if (CSR_RESULT(rc) == CSR_SUCCESS)
        {
          mutt_buffer_addstr(dest, result.data);
          FREE(&result.data);
        }
        else if ((env = myvar_get(var)))
        {
          mutt_buffer_addstr(dest, env);
        }
        else if (!(flags & MUTT_TOKEN_NOSHELL) && (env = mutt_str_getenv(var)))
        {
          mutt_buffer_addstr(dest, env);
        }
        else
        {
          mutt_buffer_addch(dest, ch);
          mutt_buffer_addstr(dest, var);
        }
        FREE(&var);
      }
    }
    else
      mutt_buffer_addch(dest, ch);
  }
  mutt_buffer_addch(dest, 0); /* terminate the string */
  SKIPWS(tok->dptr);
  return 0;
}

/**
 * mutt_opts_free - clean up before quitting
 */
void mutt_opts_free(void)
{
  clear_source_stack();

  alias_shutdown();
#ifdef USE_SIDEBAR
  sb_shutdown();
#endif

  mutt_regexlist_free(&MailLists);
  mutt_regexlist_free(&NoSpamList);
  mutt_regexlist_free(&SubscribedLists);
  mutt_regexlist_free(&UnMailLists);
  mutt_regexlist_free(&UnSubscribedLists);

  mutt_grouplist_free();
  mutt_hash_free(&TagFormats);
  mutt_hash_free(&TagTransforms);

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

  mutt_hist_free();
  mutt_keys_free();

  mutt_regexlist_free(&NoSpamList);
  mutt_commands_free();
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
  struct Buffer err = mutt_buffer_make(256);
  struct Buffer buf = mutt_buffer_make(256);

  mutt_grouplist_init();
  alias_init();
  mutt_commands_init();
#ifdef USE_COMP_MBOX
  mutt_comp_init();
#endif
#ifdef USE_IMAP
  imap_init();
#endif
#ifdef USE_LUA
  mutt_lua_init();
#endif
  TagTransforms = mutt_hash_new(64, MUTT_HASH_STRCASECMP);
  TagFormats = mutt_hash_new(64, MUTT_HASH_NO_FLAGS);

  menu_init();
  hook_init();
#ifdef USE_SIDEBAR
  sb_init();
#endif
#ifdef USE_NOTMUCH
  nm_init();
#endif

  /* "$spool_file" precedence: config file, environment */
  const char *p = mutt_str_getenv("MAIL");
  if (!p)
    p = mutt_str_getenv("MAILDIR");
  if (!p)
  {
#ifdef HOMESPOOL
    mutt_buffer_concat_path(&buf, NONULL(HomeDir), MAILPATH);
#else
    mutt_buffer_concat_path(&buf, MAILPATH, NONULL(Username));
#endif
    p = mutt_buffer_string(&buf);
  }
  cs_str_initial_set(cs, "spool_file", p, NULL);
  cs_str_reset(cs, "spool_file", NULL);

  p = mutt_str_getenv("REPLYTO");
  if (p)
  {
    struct Buffer token;

    mutt_buffer_printf(&buf, "Reply-To: %s", p);
    mutt_buffer_init(&token);
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
    cs_str_string_set(cs, "mailcap_path", env_mc, NULL);

  /* "$tmpdir" precedence: config file, environment, code */
  const char *env_tmp = mutt_str_getenv("TMPDIR");
  if (env_tmp)
    cs_str_string_set(cs, "tmpdir", env_tmp, NULL);

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

  Matches = mutt_mem_calloc(MatchesListsize, sizeof(char *));

#ifdef HAVE_GETSID
  /* Unset suspend by default if we're the session leader */
  if (getsid(0) == getpid())
    cs_subset_str_native_set(NeoMutt->sub, "suspend", false, NULL);
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
      mutt_buffer_printf(&buf, "%s/.config", HomeDir);
      xdg_cfg_home = mutt_buffer_string(&buf);
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
      mutt_buffer_strcpy(&buf, np->data);
      FREE(&np->data);
      mutt_buffer_expand_path(&buf);
      np->data = mutt_buffer_strdup(&buf);
      if (access(np->data, F_OK))
      {
        mutt_perror(np->data);
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

      mutt_buffer_printf(&buf, "%s/neomuttrc", SYSCONFDIR);
      if (access(mutt_buffer_string(&buf), F_OK) == 0)
        break;

      mutt_buffer_printf(&buf, "%s/Muttrc", SYSCONFDIR);
      if (access(mutt_buffer_string(&buf), F_OK) == 0)
        break;

      mutt_buffer_printf(&buf, "%s/neomuttrc", PKGDATADIR);
      if (access(mutt_buffer_string(&buf), F_OK) == 0)
        break;

      mutt_buffer_printf(&buf, "%s/Muttrc", PKGDATADIR);
    } while (false);

    if (access(mutt_buffer_string(&buf), F_OK) == 0)
    {
      if (source_rc(mutt_buffer_string(&buf), &err) != 0)
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

  {
    char name[256] = { 0 };
    const char *c_real_name = cs_subset_string(NeoMutt->sub, "real_name");
    if (!c_real_name)
    {
      struct passwd *pw = getpwuid(getuid());
      if (pw)
        c_real_name = mutt_gecos_name(name, sizeof(name), pw);
    }
    cs_str_initial_set(cs, "real_name", c_real_name, NULL);
    cs_str_reset(cs, "real_name", NULL);
  }

  if (need_pause && !OptNoCurses)
  {
    log_queue_flush(log_disp_terminal);
    if (mutt_any_key_to_continue(NULL) == 'q')
      goto done; // TEST14: neomutt -e broken (press 'q')
  }

  const char *const c_tmpdir = cs_subset_path(NeoMutt->sub, "tmpdir");
  mutt_file_mkdir(c_tmpdir, S_IRWXU);

  mutt_hist_init();
  mutt_hist_read_file();

#ifdef USE_NOTMUCH
  const bool c_virtual_spool_file =
      cs_subset_bool(NeoMutt->sub, "virtual_spool_file");
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
  mutt_buffer_dealloc(&err);
  mutt_buffer_dealloc(&buf);
  return rc;
}

/**
 * mutt_parse_rc_buffer - Parse a line of user config
 * @param line  config line to read
 * @param token scratch buffer to be used by parser
 * @param err   where to write error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * The reason for `token` is to avoid having to allocate and deallocate a lot
 * of memory if we are parsing many lines.  the caller can pass in the memory
 * to use, which avoids having to create new space for every call to this function.
 */
enum CommandResult mutt_parse_rc_buffer(struct Buffer *line,
                                        struct Buffer *token, struct Buffer *err)
{
  if (mutt_buffer_len(line) == 0)
    return 0;

  enum CommandResult rc = MUTT_CMD_SUCCESS;

  mutt_buffer_reset(err);

  /* Read from the beginning of line->data */
  mutt_buffer_seek(line, 0);

  SKIPWS(line->dptr);
  while (*line->dptr)
  {
    if (*line->dptr == '#')
      break; /* rest of line is a comment */
    if (*line->dptr == ';')
    {
      line->dptr++;
      continue;
    }
    mutt_extract_token(token, line, MUTT_TOKEN_NO_FLAGS);

    struct Command *cmd = NULL;
    size_t size = mutt_commands_array(&cmd);
    size_t i;
    for (i = 0; i < size; i++)
    {
      if (mutt_str_equal(token->data, cmd[i].name))
      {
        mutt_debug(LL_NOTIFY, "NT_COMMAND: %s\n", cmd[i].name);
        rc = cmd[i].parse(token, line, cmd[i].data, err);
        if (rc != MUTT_CMD_SUCCESS)
        {              /* -1 Error, +1 Finish */
          goto finish; /* Propagate return code */
        }
        notify_send(NeoMutt->notify, NT_COMMAND, i, (void *) cmd);
        break; /* Continue with next command */
      }
    }
    if (i == size)
    {
      mutt_buffer_printf(err, _("%s: unknown command"), NONULL(token->data));
      rc = MUTT_CMD_ERROR;
      break; /* Ignore the rest of the line */
    }
  }
finish:
  return rc;
}

/**
 * mutt_parse_rc_line - Parse a line of user config
 * @param line Config line to read
 * @param err  Where to write error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
enum CommandResult mutt_parse_rc_line(const char *line, struct Buffer *err)
{
  if (!line || (*line == '\0'))
    return MUTT_CMD_ERROR;

  struct Buffer *line_buffer = mutt_buffer_pool_get();
  struct Buffer *token = mutt_buffer_pool_get();

  mutt_buffer_strcpy(line_buffer, line);

  enum CommandResult rc = mutt_parse_rc_buffer(line_buffer, token, err);

  mutt_buffer_pool_release(&line_buffer);
  mutt_buffer_pool_release(&token);
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
  struct Buffer value = mutt_buffer_make(256);
  struct Buffer tmp = mutt_buffer_make(256);
  int rc = 0;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, queries, entries)
  {
    mutt_buffer_reset(&value);

    struct HashElem *he = cs_subset_lookup(NeoMutt->sub, np->data);
    if (!he)
    {
      mutt_warning(_("No such variable: %s"), np->data);
      rc = 1;
      continue;
    }

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
      mutt_buffer_reset(&tmp);
      pretty_var(value.data, &tmp);
      mutt_buffer_strcpy(&value, tmp.data);
    }

    dump_config_neo(NeoMutt->sub->cs, he, &value, NULL,
                    show_docs ? CS_DUMP_SHOW_DOCS : CS_DUMP_NO_FLAGS, stdout);
  }

  mutt_buffer_dealloc(&value);
  mutt_buffer_dealloc(&tmp);

  return rc; // TEST16: neomutt -Q charset
}

/**
 * mutt_command_complete - Complete a command name
 * @param buf     Buffer for the result
 * @param buflen  Length of the buffer
 * @param pos     Cursor position in the buffer
 * @param numtabs Number of times the user has hit 'tab'
 * @retval 1 Success, a match
 * @retval 0 Error, no match
 */
int mutt_command_complete(char *buf, size_t buflen, int pos, int numtabs)
{
  char *pt = buf;
  int spaces; /* keep track of the number of leading spaces on the line */
  struct MyVar *myv = NULL;

  SKIPWS(buf);
  spaces = buf - pt;

  pt = buf + pos - spaces;
  while ((pt > buf) && !isspace((unsigned char) *pt))
    pt--;

  if (pt == buf) /* complete cmd */
  {
    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      NumMatched = 0;
      mutt_str_copy(UserTyped, pt, sizeof(UserTyped));
      memset(Matches, 0, MatchesListsize);
      memset(Completed, 0, sizeof(Completed));

      struct Command *c = NULL;
      for (size_t num = 0, size = mutt_commands_array(&c); num < size; num++)
        candidate(UserTyped, c[num].name, Completed, sizeof(Completed));
      matches_ensure_morespace(NumMatched);
      Matches[NumMatched++] = UserTyped;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time */
      if (UserTyped[0] == '\0')
        return 1;
    }

    if ((Completed[0] == '\0') && (UserTyped[0] != '\0'))
      return 0;

    /* NumMatched will _always_ be at least 1 since the initial
     * user-typed string is always stored */
    if ((numtabs == 1) && (NumMatched == 2))
      snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
    else if ((numtabs > 1) && (NumMatched > 2))
    {
      /* cycle through all the matches */
      snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
    }

    /* return the completed command */
    strncpy(buf, Completed, buflen - spaces);
  }
  else if (mutt_str_startswith(buf, "set") || mutt_str_startswith(buf, "unset") ||
           mutt_str_startswith(buf, "reset") || mutt_str_startswith(buf, "toggle"))
  { /* complete variables */
    static const char *const prefixes[] = { "no", "inv", "?", "&", 0 };

    pt++;
    /* loop through all the possible prefixes (no, inv, ...) */
    if (mutt_str_startswith(buf, "set"))
    {
      for (int num = 0; prefixes[num]; num++)
      {
        if (mutt_str_startswith(pt, prefixes[num]))
        {
          pt += mutt_str_len(prefixes[num]);
          break;
        }
      }
    }

    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      NumMatched = 0;
      mutt_str_copy(UserTyped, pt, sizeof(UserTyped));
      memset(Matches, 0, MatchesListsize);
      memset(Completed, 0, sizeof(Completed));

      struct HashElem *he = NULL;
      struct HashElem **list = get_elem_list(NeoMutt->sub->cs);
      for (size_t i = 0; list[i]; i++)
      {
        he = list[i];
        const int type = DTYPE(he->type);

        if ((type == DT_SYNONYM) || (type & DT_DEPRECATED))
          continue;

        candidate(UserTyped, he->key.strkey, Completed, sizeof(Completed));
      }
      FREE(&list);

      TAILQ_FOREACH(myv, &MyVars, entries)
      {
        candidate(UserTyped, myv->name, Completed, sizeof(Completed));
      }
      matches_ensure_morespace(NumMatched);
      Matches[NumMatched++] = UserTyped;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time */
      if (UserTyped[0] == '\0')
        return 1;
    }

    if ((Completed[0] == 0) && UserTyped[0])
      return 0;

    /* NumMatched will _always_ be at least 1 since the initial
     * user-typed string is always stored */
    if ((numtabs == 1) && (NumMatched == 2))
      snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
    else if ((numtabs > 1) && (NumMatched > 2))
    {
      /* cycle through all the matches */
      snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
    }

    strncpy(pt, Completed, buf + buflen - pt - spaces);
  }
  else if (mutt_str_startswith(buf, "exec"))
  {
    const enum MenuType mtype = menu_get_current_type();
    const struct Binding *menu = km_get_table(mtype);
    if (!menu && (mtype != MENU_PAGER))
      menu = OpGeneric;

    pt++;
    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      NumMatched = 0;
      mutt_str_copy(UserTyped, pt, sizeof(UserTyped));
      memset(Matches, 0, MatchesListsize);
      memset(Completed, 0, sizeof(Completed));
      for (int num = 0; menu[num].name; num++)
        candidate(UserTyped, menu[num].name, Completed, sizeof(Completed));
      /* try the generic menu */
      if ((Completed[0] == '\0') && (mtype != MENU_PAGER))
      {
        menu = OpGeneric;
        for (int num = 0; menu[num].name; num++)
          candidate(UserTyped, menu[num].name, Completed, sizeof(Completed));
      }
      matches_ensure_morespace(NumMatched);
      Matches[NumMatched++] = UserTyped;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time */
      if (UserTyped[0] == '\0')
        return 1;
    }

    if ((Completed[0] == '\0') && (UserTyped[0] != '\0'))
      return 0;

    /* NumMatched will _always_ be at least 1 since the initial
     * user-typed string is always stored */
    if ((numtabs == 1) && (NumMatched == 2))
      snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
    else if ((numtabs > 1) && (NumMatched > 2))
    {
      /* cycle through all the matches */
      snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
    }

    strncpy(pt, Completed, buf + buflen - pt - spaces);
  }
  else
    return 0;

  return 1;
}

/**
 * mutt_label_complete - Complete a label name
 * @param buf     Buffer for the result
 * @param buflen  Length of the buffer
 * @param numtabs Number of times the user has hit 'tab'
 * @retval 1 Success, a match
 * @retval 0 Error, no match
 */
int mutt_label_complete(char *buf, size_t buflen, int numtabs)
{
  char *pt = buf;
  int spaces; /* keep track of the number of leading spaces on the line */

  if (!Context || !Context->mailbox->label_hash)
    return 0;

  SKIPWS(buf);
  spaces = buf - pt;

  /* first TAB. Collect all the matches */
  if (numtabs == 1)
  {
    struct HashElem *entry = NULL;
    struct HashWalkState state = { 0 };

    NumMatched = 0;
    mutt_str_copy(UserTyped, buf, sizeof(UserTyped));
    memset(Matches, 0, MatchesListsize);
    memset(Completed, 0, sizeof(Completed));
    while ((entry = mutt_hash_walk(Context->mailbox->label_hash, &state)))
      candidate(UserTyped, entry->key.strkey, Completed, sizeof(Completed));
    matches_ensure_morespace(NumMatched);
    qsort(Matches, NumMatched, sizeof(char *), (sort_t) mutt_istr_cmp);
    Matches[NumMatched++] = UserTyped;

    /* All matches are stored. Longest non-ambiguous string is ""
     * i.e. don't change 'buf'. Fake successful return this time */
    if (UserTyped[0] == '\0')
      return 1;
  }

  if ((Completed[0] == '\0') && (UserTyped[0] != '\0'))
    return 0;

  /* NumMatched will _always_ be at least 1 since the initial
   * user-typed string is always stored */
  if ((numtabs == 1) && (NumMatched == 2))
    snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
  else if ((numtabs > 1) && (NumMatched > 2))
  {
    /* cycle through all the matches */
    snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
  }

  /* return the completed label */
  strncpy(buf, Completed, buflen - spaces);

  return 1;
}

#ifdef USE_NOTMUCH
/**
 * mutt_nm_query_complete - Complete to the nearest notmuch tag
 * @param buf     Buffer for the result
 * @param buflen  Length of the buffer
 * @param pos     Cursor position in the buffer
 * @param numtabs Number of times the user has hit 'tab'
 * @retval true  Success, a match
 * @retval false Error, no match
 *
 * Complete the nearest "tag:"-prefixed string previous to pos.
 */
bool mutt_nm_query_complete(char *buf, size_t buflen, int pos, int numtabs)
{
  char *pt = buf;
  int spaces;

  SKIPWS(buf);
  spaces = buf - pt;

  pt = (char *) mutt_strn_rfind((char *) buf, pos, "tag:");
  if (pt)
  {
    pt += 4;
    if (numtabs == 1)
    {
      /* First TAB. Collect all the matches */
      complete_all_nm_tags(pt);

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time.  */
      if (UserTyped[0] == '\0')
        return true;
    }

    if ((Completed[0] == '\0') && (UserTyped[0] != '\0'))
      return false;

    /* NumMatched will _always_ be at least 1 since the initial
     * user-typed string is always stored */
    if ((numtabs == 1) && (NumMatched == 2))
      snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
    else if ((numtabs > 1) && (NumMatched > 2))
    {
      /* cycle through all the matches */
      snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
    }

    /* return the completed query */
    strncpy(pt, Completed, buf + buflen - pt - spaces);
  }
  else
    return false;

  return true;
}
#endif

#ifdef USE_NOTMUCH
/**
 * mutt_nm_tag_complete - Complete to the nearest notmuch tag
 * @param buf     Buffer for the result
 * @param buflen  Length of the buffer
 * @param numtabs Number of times the user has hit 'tab'
 * @retval true  Success, a match
 * @retval false Error, no match
 *
 * Complete the nearest "+" or "-" -prefixed string previous to pos.
 */
bool mutt_nm_tag_complete(char *buf, size_t buflen, int numtabs)
{
  if (!buf)
    return false;

  char *pt = buf;

  /* Only examine the last token */
  char *last_space = strrchr(buf, ' ');
  if (last_space)
    pt = (last_space + 1);

  /* Skip the +/- */
  if ((pt[0] == '+') || (pt[0] == '-'))
    pt++;

  if (numtabs == 1)
  {
    /* First TAB. Collect all the matches */
    complete_all_nm_tags(pt);

    /* All matches are stored. Longest non-ambiguous string is ""
     * i.e. don't change 'buf'. Fake successful return this time.  */
    if (UserTyped[0] == '\0')
      return true;
  }

  if ((Completed[0] == '\0') && (UserTyped[0] != '\0'))
    return false;

  /* NumMatched will _always_ be at least 1 since the initial
   * user-typed string is always stored */
  if ((numtabs == 1) && (NumMatched == 2))
    snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
  else if ((numtabs > 1) && (NumMatched > 2))
  {
    /* cycle through all the matches */
    snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
  }

  /* return the completed query */
  strncpy(pt, Completed, buf + buflen - pt);

  return true;
}
#endif

/**
 * mutt_var_value_complete - Complete a variable/value
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 * @param pos    Cursor position in the buffer
 */
int mutt_var_value_complete(char *buf, size_t buflen, int pos)
{
  char *pt = buf;

  if (buf[0] == '\0')
    return 0;

  SKIPWS(buf);
  const int spaces = buf - pt;

  pt = buf + pos - spaces;
  while ((pt > buf) && !isspace((unsigned char) *pt))
    pt--;
  pt++;           /* move past the space */
  if (*pt == '=') /* abort if no var before the '=' */
    return 0;

  if (mutt_str_startswith(buf, "set"))
  {
    const char *myvarval = NULL;
    char var[256];
    mutt_str_copy(var, pt, sizeof(var));
    /* ignore the trailing '=' when comparing */
    int vlen = mutt_str_len(var);
    if (vlen == 0)
      return 0;

    var[vlen - 1] = '\0';

    struct HashElem *he = cs_subset_lookup(NeoMutt->sub, var);
    if (!he)
    {
      myvarval = myvar_get(var);
      if (myvarval)
      {
        struct Buffer pretty = mutt_buffer_make(256);
        pretty_var(myvarval, &pretty);
        snprintf(pt, buflen - (pt - buf), "%s=%s", var, pretty.data);
        mutt_buffer_dealloc(&pretty);
        return 1;
      }
      return 0; /* no such variable. */
    }
    else
    {
      struct Buffer value = mutt_buffer_make(256);
      struct Buffer pretty = mutt_buffer_make(256);
      int rc = cs_subset_he_string_get(NeoMutt->sub, he, &value);
      if (CSR_RESULT(rc) == CSR_SUCCESS)
      {
        pretty_var(value.data, &pretty);
        snprintf(pt, buflen - (pt - buf), "%s=%s", var, pretty.data);
        mutt_buffer_dealloc(&value);
        mutt_buffer_dealloc(&pretty);
        return 0;
      }
      mutt_buffer_dealloc(&value);
      mutt_buffer_dealloc(&pretty);
      return 1;
    }
  }
  return 0;
}
