/**
 * @file
 * Functions to parse commands in a config file
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2010,2012-2013,2016 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
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
 * @page neo_command_parse Functions to parse commands in a config file
 *
 * Functions to parse commands in a config file
 */

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "command_parse.h"
#include "imap/lib.h"
#include "menu/lib.h"
#include "init.h"
#include "keymap.h"
#include "monitor.h"
#include "mutt_commands.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "mx.h"
#include "myvar.h"
#include "options.h"
#include "version.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

/* LIFO designed to contain the list of config files that have been sourced and
 * avoid cyclic sourcing */
static struct ListHead MuttrcStack = STAILQ_HEAD_INITIALIZER(MuttrcStack);

#define MAX_ERRS 128

/**
 * enum GroupState - Type of email address group
 */
enum GroupState
{
  GS_NONE, ///< Group is missing an argument
  GS_RX,   ///< Entry is a regular expression
  GS_ADDR, ///< Entry is an address
};

/**
 * is_function - Is the argument a neomutt function?
 * @param name  Command name to be searched for
 * @retval true  Function found
 * @retval false Function not found
 */
static bool is_function(const char *name)
{
  for (size_t i = 0; MenuNames[i].name; i++)
  {
    const struct Binding *b = km_get_table(MenuNames[i].value);
    if (!b)
      continue;

    for (int j = 0; b[j].name; j++)
      if (mutt_str_equal(name, b[j].name))
        return true;
  }
  return false;
}

/**
 * parse_grouplist - Parse a group context
 * @param gl   GroupList to add to
 * @param buf  Temporary Buffer space
 * @param s    Buffer containing string to be parsed
 * @param err  Buffer for error messages
 * @retval  0 Success
 * @retval -1 Error
 */
int parse_grouplist(struct GroupList *gl, struct Buffer *buf, struct Buffer *s,
                    struct Buffer *err)
{
  while (mutt_istr_equal(buf->data, "-group"))
  {
    if (!MoreArgs(s))
    {
      mutt_buffer_strcpy(err, _("-group: no group name"));
      return -1;
    }

    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

    mutt_grouplist_add(gl, mutt_pattern_group(buf->data));

    if (!MoreArgs(s))
    {
      mutt_buffer_strcpy(err, _("out of arguments"));
      return -1;
    }

    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
  }

  return 0;
}

/**
 * source_rc - Read an initialization file
 * @param rcfile_path Path to initialization file
 * @param err         Buffer for error messages
 * @retval <0 NeoMutt should pause to let the user know
 */
int source_rc(const char *rcfile_path, struct Buffer *err)
{
  int lineno = 0, rc = 0, warnings = 0;
  enum CommandResult line_rc;
  struct Buffer *token = NULL, *linebuf = NULL;
  char *line = NULL;
  char *currentline = NULL;
  char rcfile[PATH_MAX];
  size_t linelen = 0;
  pid_t pid;

  mutt_str_copy(rcfile, rcfile_path, sizeof(rcfile));

  size_t rcfilelen = mutt_str_len(rcfile);
  if (rcfilelen == 0)
    return -1;

  bool ispipe = rcfile[rcfilelen - 1] == '|';

  if (!ispipe)
  {
    struct ListNode *np = STAILQ_FIRST(&MuttrcStack);
    if (!mutt_path_to_absolute(rcfile, np ? NONULL(np->data) : ""))
    {
      mutt_error(_("Error: Can't build path of '%s'"), rcfile_path);
      return -1;
    }

    STAILQ_FOREACH(np, &MuttrcStack, entries)
    {
      if (mutt_str_equal(np->data, rcfile))
      {
        break;
      }
    }
    if (np)
    {
      mutt_error(_("Error: Cyclic sourcing of configuration file '%s'"), rcfile);
      return -1;
    }

    mutt_list_insert_head(&MuttrcStack, mutt_str_dup(rcfile));
  }

  mutt_debug(LL_DEBUG2, "Reading configuration file '%s'\n", rcfile);

  FILE *fp = mutt_open_read(rcfile, &pid);
  if (!fp)
  {
    mutt_buffer_printf(err, "%s: %s", rcfile, strerror(errno));
    return -1;
  }

  token = mutt_buffer_pool_get();
  linebuf = mutt_buffer_pool_get();

  while ((line = mutt_file_read_line(line, &linelen, fp, &lineno, MUTT_RL_CONT)) != NULL)
  {
    const char *const c_config_charset =
        cs_subset_string(NeoMutt->sub, "config_charset");
    const char *const c_charset = cs_subset_string(NeoMutt->sub, "charset");
    const bool conv = c_config_charset && c_charset;
    if (conv)
    {
      currentline = mutt_str_dup(line);
      if (!currentline)
        continue;
      mutt_ch_convert_string(&currentline, c_config_charset, c_charset, MUTT_ICONV_NO_FLAGS);
    }
    else
      currentline = line;

    mutt_buffer_strcpy(linebuf, currentline);

    mutt_buffer_reset(err);
    line_rc = mutt_parse_rc_buffer(linebuf, token, err);
    if (line_rc == MUTT_CMD_ERROR)
    {
      mutt_error(_("Error in %s, line %d: %s"), rcfile, lineno, err->data);
      if (--rc < -MAX_ERRS)
      {
        if (conv)
          FREE(&currentline);
        break;
      }
    }
    else if (line_rc == MUTT_CMD_WARNING)
    {
      /* Warning */
      mutt_warning(_("Warning in %s, line %d: %s"), rcfile, lineno, err->data);
      warnings++;
    }
    else if (line_rc == MUTT_CMD_FINISH)
    {
      break; /* Found "finish" command */
    }
    else
    {
      if (rc < 0)
        rc = -1;
    }
    if (conv)
      FREE(&currentline);
  }

  FREE(&line);
  mutt_file_fclose(&fp);
  if (pid != -1)
    filter_wait(pid);

  if (rc)
  {
    /* the neomuttrc source keyword */
    mutt_buffer_reset(err);
    mutt_buffer_printf(err, (rc >= -MAX_ERRS) ? _("source: errors in %s") : _("source: reading aborted due to too many errors in %s"),
                       rcfile);
    rc = -1;
  }
  else
  {
    /* Don't alias errors with warnings */
    if (warnings > 0)
    {
      mutt_buffer_printf(err, ngettext("source: %d warning in %s", "source: %d warnings in %s", warnings),
                         warnings, rcfile);
      rc = -2;
    }
  }

  if (!ispipe && !STAILQ_EMPTY(&MuttrcStack))
  {
    struct ListNode *np = STAILQ_FIRST(&MuttrcStack);
    STAILQ_REMOVE_HEAD(&MuttrcStack, entries);
    FREE(&np->data);
    FREE(&np);
  }

  mutt_buffer_pool_release(&token);
  mutt_buffer_pool_release(&linebuf);
  return rc;
}

/**
 * parse_cd - Parse the 'cd' command - Implements Command::parse()
 */
enum CommandResult parse_cd(struct Buffer *buf, struct Buffer *s, intptr_t data,
                            struct Buffer *err)
{
  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
  mutt_buffer_expand_path(buf);
  if (mutt_buffer_len(buf) == 0)
  {
    if (HomeDir)
      mutt_buffer_strcpy(buf, HomeDir);
    else
    {
      mutt_buffer_printf(err, _("%s: too few arguments"), "cd");
      return MUTT_CMD_ERROR;
    }
  }

  if (chdir(mutt_buffer_string(buf)) != 0)
  {
    mutt_buffer_printf(err, "cd: %s", strerror(errno));
    return MUTT_CMD_ERROR;
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_echo - Parse the 'echo' command - Implements Command::parse()
 */
enum CommandResult parse_echo(struct Buffer *buf, struct Buffer *s,
                              intptr_t data, struct Buffer *err)
{
  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "echo");
    return MUTT_CMD_WARNING;
  }
  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
  OptForceRefresh = true;
  mutt_message("%s", buf->data);
  OptForceRefresh = false;
  mutt_sleep(0);

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_finish - Parse the 'finish' command - Implements Command::parse()
 * @retval  #MUTT_CMD_FINISH Stop processing the current file
 * @retval  #MUTT_CMD_WARNING Failed
 *
 * If the 'finish' command is found, we should stop reading the current file.
 */
enum CommandResult parse_finish(struct Buffer *buf, struct Buffer *s,
                                intptr_t data, struct Buffer *err)
{
  if (MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too many arguments"), "finish");
    return MUTT_CMD_WARNING;
  }

  return MUTT_CMD_FINISH;
}

/**
 * parse_group - Parse the 'group' and 'ungroup' commands - Implements Command::parse()
 */
enum CommandResult parse_group(struct Buffer *buf, struct Buffer *s,
                               intptr_t data, struct Buffer *err)
{
  struct GroupList gl = STAILQ_HEAD_INITIALIZER(gl);
  enum GroupState state = GS_NONE;

  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    if (parse_grouplist(&gl, buf, s, err) == -1)
      goto bail;

    if ((data == MUTT_UNGROUP) && mutt_istr_equal(buf->data, "*"))
    {
      mutt_grouplist_clear(&gl);
      goto out;
    }

    if (mutt_istr_equal(buf->data, "-rx"))
      state = GS_RX;
    else if (mutt_istr_equal(buf->data, "-addr"))
      state = GS_ADDR;
    else
    {
      switch (state)
      {
        case GS_NONE:
          mutt_buffer_printf(err, _("%sgroup: missing -rx or -addr"),
                             (data == MUTT_UNGROUP) ? "un" : "");
          goto warn;

        case GS_RX:
          if ((data == MUTT_GROUP) &&
              (mutt_grouplist_add_regex(&gl, buf->data, REG_ICASE, err) != 0))
          {
            goto bail;
          }
          else if ((data == MUTT_UNGROUP) &&
                   (mutt_grouplist_remove_regex(&gl, buf->data) < 0))
          {
            goto bail;
          }
          break;

        case GS_ADDR:
        {
          char *estr = NULL;
          struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
          mutt_addrlist_parse2(&al, buf->data);
          if (TAILQ_EMPTY(&al))
            goto bail;
          if (mutt_addrlist_to_intl(&al, &estr))
          {
            mutt_buffer_printf(err, _("%sgroup: warning: bad IDN '%s'"),
                               (data == 1) ? "un" : "", estr);
            mutt_addrlist_clear(&al);
            FREE(&estr);
            goto bail;
          }
          if (data == MUTT_GROUP)
            mutt_grouplist_add_addrlist(&gl, &al);
          else if (data == MUTT_UNGROUP)
            mutt_grouplist_remove_addrlist(&gl, &al);
          mutt_addrlist_clear(&al);
          break;
        }
      }
    }
  } while (MoreArgs(s));

out:
  mutt_grouplist_destroy(&gl);
  return MUTT_CMD_SUCCESS;

bail:
  mutt_grouplist_destroy(&gl);
  return MUTT_CMD_ERROR;

warn:
  mutt_grouplist_destroy(&gl);
  return MUTT_CMD_WARNING;
}

/**
 * parse_ifdef - Parse the 'ifdef' and 'ifndef' commands - Implements Command::parse()
 *
 * The 'ifdef' command allows conditional elements in the config file.
 * If a given variable, function, command or compile-time symbol exists, then
 * read the rest of the line of config commands.
 * e.g.
 *      ifdef sidebar source ~/.neomutt/sidebar.rc
 *
 * If (data == 1) then it means use the 'ifndef' (if-not-defined) command.
 * e.g.
 *      ifndef imap finish
 */
enum CommandResult parse_ifdef(struct Buffer *buf, struct Buffer *s,
                               intptr_t data, struct Buffer *err)
{
  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

  // is the item defined as:
  bool res = cs_subset_lookup(NeoMutt->sub, buf->data) // a variable?
             || feature_enabled(buf->data)             // a compiled-in feature?
             || is_function(buf->data)                 // a function?
             || mutt_command_get(buf->data)            // a command?
             || myvar_get(buf->data)                   // a my_ variable?
             || mutt_str_getenv(buf->data); // an environment variable?

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), (data ? "ifndef" : "ifdef"));
    return MUTT_CMD_WARNING;
  }
  mutt_extract_token(buf, s, MUTT_TOKEN_SPACE);

  /* ifdef KNOWN_SYMBOL or ifndef UNKNOWN_SYMBOL */
  if ((res && (data == 0)) || (!res && (data == 1)))
  {
    enum CommandResult rc = mutt_parse_rc_line(buf->data, err);
    if (rc == MUTT_CMD_ERROR)
    {
      mutt_error(_("Error: %s"), err->data);
      return MUTT_CMD_ERROR;
    }
    return rc;
  }
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_ignore - Parse the 'ignore' command - Implements Command::parse()
 */
enum CommandResult parse_ignore(struct Buffer *buf, struct Buffer *s,
                                intptr_t data, struct Buffer *err)
{
  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    remove_from_stailq(&UnIgnore, buf->data);
    add_to_stailq(&Ignore, buf->data);
  } while (MoreArgs(s));

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_lists - Parse the 'lists' command - Implements Command::parse()
 */
enum CommandResult parse_lists(struct Buffer *buf, struct Buffer *s,
                               intptr_t data, struct Buffer *err)
{
  struct GroupList gl = STAILQ_HEAD_INITIALIZER(gl);

  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

    if (parse_grouplist(&gl, buf, s, err) == -1)
      goto bail;

    mutt_regexlist_remove(&UnMailLists, buf->data);

    if (mutt_regexlist_add(&MailLists, buf->data, REG_ICASE, err) != 0)
      goto bail;

    if (mutt_grouplist_add_regex(&gl, buf->data, REG_ICASE, err) != 0)
      goto bail;
  } while (MoreArgs(s));

  mutt_grouplist_destroy(&gl);
  return MUTT_CMD_SUCCESS;

bail:
  mutt_grouplist_destroy(&gl);
  return MUTT_CMD_ERROR;
}

/**
 * parse_mailboxes - Parse the 'mailboxes' command - Implements Command::parse()
 *
 * This is also used by 'virtual-mailboxes'.
 */
enum CommandResult parse_mailboxes(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  if (data & MUTT_COMMAND_DEPRECATED)
  {
    mutt_warning(_("Command '%s' is deprecated"), buf->data);
    rc = MUTT_CMD_WARNING;
  }

  while (MoreArgs(s))
  {
    struct Mailbox *m = mailbox_new();

    if (data & MUTT_NAMED)
    {
      // This may be empty, e.g. `named-mailboxes "" +inbox`
      mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
      m->name = mutt_buffer_strdup(buf);
    }

    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    if (mutt_buffer_is_empty(buf))
    {
      /* Skip empty tokens. */
      mailbox_free(&m);
      continue;
    }

    mutt_buffer_strcpy(&m->pathbuf, buf->data);
    const char *const c_folder = cs_subset_string(NeoMutt->sub, "folder");
    /* int rc = */ mx_path_canon2(m, c_folder);

    if (m->type <= MUTT_UNKNOWN)
    {
      mutt_error("Unknown Mailbox: %s", m->realpath);
      mailbox_free(&m);
      return MUTT_CMD_ERROR;
    }

    bool new_account = false;
    struct Account *a = mx_ac_find(m);
    if (!a)
    {
      a = account_new(NULL, NeoMutt->sub);
      a->type = m->type;
      new_account = true;
    }

    if (!new_account)
    {
      struct Mailbox *m_old = mx_mbox_find(a, m->realpath);
      if (m_old)
      {
        const bool show = (m_old->flags == MB_HIDDEN);
        if (show)
        {
          m_old->flags = MB_NORMAL;
          m_old->gen = mailbox_gen();
        }

        const bool rename = (data & MUTT_NAMED) && !mutt_str_equal(m_old->name, m->name);
        if (rename)
        {
          mutt_str_replace(&m_old->name, m->name);
        }

        mailbox_free(&m);
        continue;
      }
    }

    if (!mx_ac_add(a, m))
    {
      //error
      mailbox_free(&m);
      if (new_account)
      {
        cs_subset_free(&a->sub);
        FREE(&a->name);
        notify_free(&a->notify);
        FREE(&a);
      }
      continue;
    }
    if (new_account)
    {
      neomutt_account_add(NeoMutt, a);
    }

#ifdef USE_INOTIFY
    mutt_monitor_add(m);
#endif
  }
  return rc;
}

/**
 * parse_my_hdr - Parse the 'my_hdr' command - Implements Command::parse()
 */
enum CommandResult parse_my_hdr(struct Buffer *buf, struct Buffer *s,
                                intptr_t data, struct Buffer *err)
{
  mutt_extract_token(buf, s, MUTT_TOKEN_SPACE | MUTT_TOKEN_QUOTE);
  char *p = strpbrk(buf->data, ": \t");
  if (!p || (*p != ':'))
  {
    mutt_buffer_strcpy(err, _("invalid header field"));
    return MUTT_CMD_WARNING;
  }

  struct EventHeader ev_h = { buf->data };
  struct ListNode *n = header_find(&UserHeader, buf->data);

  if (n)
  {
    header_update(n, buf->data);
    mutt_debug(LL_NOTIFY, "NT_HEADER_CHANGE: %s\n", buf->data);
    notify_send(NeoMutt->notify, NT_HEADER, NT_HEADER_CHANGE, &ev_h);
  }
  else
  {
    header_add(&UserHeader, buf->data);
    mutt_debug(LL_NOTIFY, "NT_HEADER_ADD: %s\n", buf->data);
    notify_send(NeoMutt->notify, NT_HEADER, NT_HEADER_ADD, &ev_h);
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_set - Parse the 'set' family of commands - Implements Command::parse()
 *
 * This is used by 'reset', 'set', 'toggle' and 'unset'.
 */
enum CommandResult parse_set(struct Buffer *buf, struct Buffer *s,
                             intptr_t data, struct Buffer *err)
{
  /* The order must match `enum MuttSetCommand` */
  static const char *set_commands[] = { "set", "toggle", "unset", "reset" };

  int rc = 0;

  while (MoreArgs(s))
  {
    bool prefix = false;
    bool query = false;
    bool inv = (data == MUTT_SET_INV);
    bool reset = (data == MUTT_SET_RESET);
    bool unset = (data == MUTT_SET_UNSET);

    if (*s->dptr == '?')
    {
      prefix = true;
      query = true;
      s->dptr++;
    }
    else if (mutt_str_startswith(s->dptr, "no"))
    {
      prefix = true;
      unset = !unset;
      s->dptr += 2;
    }
    else if (mutt_str_startswith(s->dptr, "inv"))
    {
      prefix = true;
      inv = !inv;
      s->dptr += 3;
    }
    else if (*s->dptr == '&')
    {
      prefix = true;
      reset = true;
      s->dptr++;
    }

    if (prefix && (data != MUTT_SET_SET))
    {
      mutt_buffer_printf(err, _("Can't use 'inv', 'no', '&' or '?' with the '%s' command"),
                         set_commands[data]);
      return MUTT_CMD_WARNING;
    }

    /* get the variable name */
    mutt_extract_token(buf, s, MUTT_TOKEN_EQUAL | MUTT_TOKEN_QUESTION | MUTT_TOKEN_PLUS | MUTT_TOKEN_MINUS);

    bool bq = false;
    bool equals = false;
    bool increment = false;
    bool decrement = false;

    struct HashElem *he = NULL;
    bool my = mutt_str_startswith(buf->data, "my_");
    if (!my)
    {
      he = cs_subset_lookup(NeoMutt->sub, buf->data);
      if (!he)
      {
        if (reset && mutt_str_equal(buf->data, "all"))
        {
          struct HashElem **list = get_elem_list(NeoMutt->sub->cs);
          if (!list)
            return MUTT_CMD_ERROR;

          for (size_t i = 0; list[i]; i++)
            cs_subset_he_reset(NeoMutt->sub, list[i], NULL);

          FREE(&list);
          break;
        }
        else
        {
          mutt_buffer_printf(err, _("%s: unknown variable"), buf->data);
          return MUTT_CMD_ERROR;
        }
      }

      // Use the correct name if a synonym is used
      mutt_buffer_strcpy(buf, he->key.strkey);

      bq = ((DTYPE(he->type) == DT_BOOL) || (DTYPE(he->type) == DT_QUAD));
    }

    if (*s->dptr == '?')
    {
      if (prefix)
      {
        mutt_buffer_printf(err,
                           _("Can't use a prefix when querying a variable"));
        return MUTT_CMD_WARNING;
      }

      if (reset || unset || inv)
      {
        mutt_buffer_printf(err, _("Can't query a variable with the '%s' command"),
                           set_commands[data]);
        return MUTT_CMD_WARNING;
      }

      query = true;
      s->dptr++;
    }
    else if (*s->dptr == '+' || *s->dptr == '-')
    {
      if (prefix)
      {
        mutt_buffer_printf(
            err,
            _("Can't use prefix when incrementing or decrementing a variable"));
        return MUTT_CMD_WARNING;
      }

      if (reset || unset || inv)
      {
        mutt_buffer_printf(err, _("Can't set a variable with the '%s' command"),
                           set_commands[data]);
        return MUTT_CMD_WARNING;
      }
      if (*s->dptr == '+')
        increment = true;
      else
        decrement = true;

      if (my && decrement)
      {
        mutt_buffer_printf(err, _("Can't decrement a my_ variable"), set_commands[data]);
        return MUTT_CMD_WARNING;
      }
      s->dptr++;
      if (*s->dptr == '=')
      {
        equals = true;
        s->dptr++;
      }
    }
    else if (*s->dptr == '=')
    {
      if (prefix)
      {
        mutt_buffer_printf(err, _("Can't use prefix when setting a variable"));
        return MUTT_CMD_WARNING;
      }

      if (reset || unset || inv)
      {
        mutt_buffer_printf(err, _("Can't set a variable with the '%s' command"),
                           set_commands[data]);
        return MUTT_CMD_WARNING;
      }

      equals = true;
      s->dptr++;
    }

    if (!bq && (inv || (unset && prefix)))
    {
      if (data == MUTT_SET_SET)
      {
        mutt_buffer_printf(err, _("Prefixes 'no' and 'inv' may only be used "
                                  "with bool/quad variables"));
      }
      else
      {
        mutt_buffer_printf(err, _("Command '%s' can only be used with bool/quad variables"),
                           set_commands[data]);
      }
      return MUTT_CMD_WARNING;
    }

    if (reset)
    {
      // mutt_buffer_printf(err, "ACT24 reset variable %s", buf->data);
      if (he)
      {
        rc = cs_subset_he_reset(NeoMutt->sub, he, err);
        if (CSR_RESULT(rc) != CSR_SUCCESS)
          return MUTT_CMD_ERROR;
      }
      else
      {
        myvar_del(buf->data);
      }
      continue;
    }

    if ((data == MUTT_SET_SET) && !inv && !unset)
    {
      if (query)
      {
        // mutt_buffer_printf(err, "ACT08 query variable %s", buf->data);
        if (he)
        {
          mutt_buffer_addstr(err, buf->data);
          mutt_buffer_addch(err, '=');
          mutt_buffer_reset(buf);
          rc = cs_subset_he_string_get(NeoMutt->sub, he, buf);
          if (CSR_RESULT(rc) != CSR_SUCCESS)
          {
            mutt_buffer_addstr(err, buf->data);
            return MUTT_CMD_ERROR;
          }
          if (DTYPE(he->type) == DT_PATH)
            mutt_pretty_mailbox(buf->data, buf->dsize);
          pretty_var(buf->data, err);
        }
        else
        {
          const char *val = myvar_get(buf->data);
          if (val)
          {
            mutt_buffer_addstr(err, buf->data);
            mutt_buffer_addch(err, '=');
            pretty_var(val, err);
          }
          else
          {
            mutt_buffer_printf(err, _("%s: unknown variable"), buf->data);
            return MUTT_CMD_ERROR;
          }
        }
        break;
      }
      else if (equals)
      {
        // mutt_buffer_printf(err, "ACT11 set variable %s to ", buf->data);
        const char *name = NULL;
        if (my)
        {
          name = mutt_str_dup(buf->data);
        }
        mutt_extract_token(buf, s, MUTT_TOKEN_BACKTICK_VARS);
        if (my)
        {
          assert(!decrement);
          if (increment)
          {
            myvar_append(name, buf->data);
          }
          else
          {
            myvar_set(name, buf->data);
          }
          FREE(&name);
        }
        else
        {
          if (DTYPE(he->type) == DT_PATH)
          {
            if (he->type & (DT_PATH_DIR | DT_PATH_FILE))
              mutt_buffer_expand_path(buf);
            else
              mutt_path_tilde(buf->data, buf->dsize, HomeDir);
          }
          else if (IS_MAILBOX(he))
          {
            mutt_buffer_expand_path(buf);
          }
          else if (IS_COMMAND(he))
          {
            struct Buffer scratch = mutt_buffer_make(1024);
            mutt_buffer_copy(&scratch, buf);

            if (!mutt_str_equal(buf->data, "builtin"))
            {
              mutt_buffer_expand_path(&scratch);
            }
            mutt_buffer_reset(buf);
            mutt_buffer_addstr(buf, mutt_buffer_string(&scratch));
            mutt_buffer_dealloc(&scratch);
          }
          if (increment)
          {
            rc = cs_subset_he_string_plus_equals(NeoMutt->sub, he, buf->data, err);
          }
          else if (decrement)
          {
            rc = cs_subset_he_string_minus_equals(NeoMutt->sub, he, buf->data, err);
          }
          else
          {
            rc = cs_subset_he_string_set(NeoMutt->sub, he, buf->data, err);
          }
          if (CSR_RESULT(rc) != CSR_SUCCESS)
            return MUTT_CMD_ERROR;
        }
        continue;
      }
      else
      {
        if (bq)
        {
          // mutt_buffer_printf(err, "ACT23 set variable %s to 'yes'", buf->data);
          rc = cs_subset_he_native_set(NeoMutt->sub, he, true, err);
          if (CSR_RESULT(rc) != CSR_SUCCESS)
            return MUTT_CMD_ERROR;
          continue;
        }
        else
        {
          // mutt_buffer_printf(err, "ACT10 query variable %s", buf->data);
          if (he)
          {
            mutt_buffer_addstr(err, buf->data);
            mutt_buffer_addch(err, '=');
            mutt_buffer_reset(buf);
            rc = cs_subset_he_string_get(NeoMutt->sub, he, buf);
            if (CSR_RESULT(rc) != CSR_SUCCESS)
            {
              mutt_buffer_addstr(err, buf->data);
              return MUTT_CMD_ERROR;
            }
            if (DTYPE(he->type) == DT_PATH)
              mutt_pretty_mailbox(buf->data, buf->dsize);
            pretty_var(buf->data, err);
          }
          else
          {
            const char *val = myvar_get(buf->data);
            if (val)
            {
              mutt_buffer_addstr(err, buf->data);
              mutt_buffer_addch(err, '=');
              pretty_var(val, err);
            }
            else
            {
              mutt_buffer_printf(err, _("%s: unknown variable"), buf->data);
              return MUTT_CMD_ERROR;
            }
          }
          break;
        }
      }
    }

    if (my)
    {
      myvar_del(buf->data);
    }
    else if (bq)
    {
      if (inv)
      {
        // mutt_buffer_printf(err, "ACT25 TOGGLE bool/quad variable %s", buf->data);
        if (DTYPE(he->type) == DT_BOOL)
          bool_he_toggle(NeoMutt->sub, he, err);
        else
          quad_he_toggle(NeoMutt->sub, he, err);
      }
      else
      {
        // mutt_buffer_printf(err, "ACT26 UNSET bool/quad variable %s", buf->data);
        rc = cs_subset_he_native_set(NeoMutt->sub, he, false, err);
        if (CSR_RESULT(rc) != CSR_SUCCESS)
          return MUTT_CMD_ERROR;
      }
      continue;
    }
    else
    {
      rc = cs_subset_he_string_set(NeoMutt->sub, he, NULL, err);
      if (CSR_RESULT(rc) != CSR_SUCCESS)
        return MUTT_CMD_ERROR;
    }
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_setenv - Parse the 'setenv' and 'unsetenv' commands - Implements Command::parse()
 */
enum CommandResult parse_setenv(struct Buffer *buf, struct Buffer *s,
                                intptr_t data, struct Buffer *err)
{
  char **envp = mutt_envlist_getlist();

  bool query = false;
  bool prefix = false;
  bool unset = (data == MUTT_SET_UNSET);

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "setenv");
    return MUTT_CMD_WARNING;
  }

  if (*s->dptr == '?')
  {
    query = true;
    prefix = true;

    if (unset)
    {
      mutt_buffer_printf(err, _("Can't query a variable with the '%s' command"), "unsetenv");
      return MUTT_CMD_WARNING;
    }

    s->dptr++;
  }

  /* get variable name */
  mutt_extract_token(buf, s, MUTT_TOKEN_EQUAL | MUTT_TOKEN_QUESTION);

  if (*s->dptr == '?')
  {
    if (unset)
    {
      mutt_buffer_printf(err, _("Can't query a variable with the '%s' command"), "unsetenv");
      return MUTT_CMD_WARNING;
    }

    if (prefix)
    {
      mutt_buffer_printf(err, _("Can't use a prefix when querying a variable"));
      return MUTT_CMD_WARNING;
    }

    query = true;
    s->dptr++;
  }

  if (query)
  {
    bool found = false;
    while (envp && *envp)
    {
      /* This will display all matches for "^QUERY" */
      if (mutt_str_startswith(*envp, buf->data))
      {
        if (!found)
        {
          mutt_endwin();
          found = true;
        }
        puts(*envp);
      }
      envp++;
    }

    if (found)
    {
      mutt_any_key_to_continue(NULL);
      return MUTT_CMD_SUCCESS;
    }

    mutt_buffer_printf(err, _("%s is unset"), buf->data);
    return MUTT_CMD_WARNING;
  }

  if (unset)
  {
    if (mutt_envlist_unset(buf->data))
      return MUTT_CMD_SUCCESS;
    return MUTT_CMD_ERROR;
  }

  /* set variable */

  if (*s->dptr == '=')
  {
    s->dptr++;
    SKIPWS(s->dptr);
  }

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "setenv");
    return MUTT_CMD_WARNING;
  }

  char *name = mutt_str_dup(buf->data);
  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
  mutt_envlist_set(name, buf->data, true);
  FREE(&name);

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_source - Parse the 'source' command - Implements Command::parse()
 */
enum CommandResult parse_source(struct Buffer *buf, struct Buffer *s,
                                intptr_t data, struct Buffer *err)
{
  char path[PATH_MAX];

  do
  {
    if (mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS) != 0)
    {
      mutt_buffer_printf(err, _("source: error at %s"), s->dptr);
      return MUTT_CMD_ERROR;
    }
    mutt_str_copy(path, buf->data, sizeof(path));
    mutt_expand_path(path, sizeof(path));

    if (source_rc(path, err) < 0)
    {
      mutt_buffer_printf(err, _("source: file %s could not be sourced"), path);
      return MUTT_CMD_ERROR;
    }

  } while (MoreArgs(s));

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_spam_list - Parse the 'spam' and 'nospam' commands - Implements Command::parse()
 */
enum CommandResult parse_spam_list(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  struct Buffer templ;

  mutt_buffer_init(&templ);

  /* Insist on at least one parameter */
  if (!MoreArgs(s))
  {
    if (data == MUTT_SPAM)
      mutt_buffer_strcpy(err, _("spam: no matching pattern"));
    else
      mutt_buffer_strcpy(err, _("nospam: no matching pattern"));
    return MUTT_CMD_ERROR;
  }

  /* Extract the first token, a regex */
  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

  /* data should be either MUTT_SPAM or MUTT_NOSPAM. MUTT_SPAM is for spam commands. */
  if (data == MUTT_SPAM)
  {
    /* If there's a second parameter, it's a template for the spam tag. */
    if (MoreArgs(s))
    {
      mutt_extract_token(&templ, s, MUTT_TOKEN_NO_FLAGS);

      /* Add to the spam list. */
      if (mutt_replacelist_add(&SpamList, buf->data, templ.data, err) != 0)
      {
        FREE(&templ.data);
        return MUTT_CMD_ERROR;
      }
      FREE(&templ.data);
    }
    /* If not, try to remove from the nospam list. */
    else
    {
      mutt_regexlist_remove(&NoSpamList, buf->data);
    }

    return MUTT_CMD_SUCCESS;
  }
  /* MUTT_NOSPAM is for nospam commands. */
  else if (data == MUTT_NOSPAM)
  {
    /* nospam only ever has one parameter. */

    /* "*" is a special case. */
    if (mutt_str_equal(buf->data, "*"))
    {
      mutt_replacelist_free(&SpamList);
      mutt_regexlist_free(&NoSpamList);
      return MUTT_CMD_SUCCESS;
    }

    /* If it's on the spam list, just remove it. */
    if (mutt_replacelist_remove(&SpamList, buf->data) != 0)
      return MUTT_CMD_SUCCESS;

    /* Otherwise, add it to the nospam list. */
    if (mutt_regexlist_add(&NoSpamList, buf->data, REG_ICASE, err) != 0)
      return MUTT_CMD_ERROR;

    return MUTT_CMD_SUCCESS;
  }

  /* This should not happen. */
  mutt_buffer_strcpy(err, "This is no good at all.");
  return MUTT_CMD_ERROR;
}

/**
 * parse_stailq - Parse a list command - Implements Command::parse()
 *
 * This is used by 'alternative_order', 'auto_view' and several others.
 */
enum CommandResult parse_stailq(struct Buffer *buf, struct Buffer *s,
                                intptr_t data, struct Buffer *err)
{
  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    add_to_stailq((struct ListHead *) data, buf->data);
  } while (MoreArgs(s));

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_subscribe - Parse the 'subscribe' command - Implements Command::parse()
 */
enum CommandResult parse_subscribe(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  struct GroupList gl = STAILQ_HEAD_INITIALIZER(gl);

  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

    if (parse_grouplist(&gl, buf, s, err) == -1)
      goto bail;

    mutt_regexlist_remove(&UnMailLists, buf->data);
    mutt_regexlist_remove(&UnSubscribedLists, buf->data);

    if (mutt_regexlist_add(&MailLists, buf->data, REG_ICASE, err) != 0)
      goto bail;
    if (mutt_regexlist_add(&SubscribedLists, buf->data, REG_ICASE, err) != 0)
      goto bail;
    if (mutt_grouplist_add_regex(&gl, buf->data, REG_ICASE, err) != 0)
      goto bail;
  } while (MoreArgs(s));

  mutt_grouplist_destroy(&gl);
  return MUTT_CMD_SUCCESS;

bail:
  mutt_grouplist_destroy(&gl);
  return MUTT_CMD_ERROR;
}

#ifdef USE_IMAP
/**
 * parse_subscribe_to - Parse the 'subscribe-to' command - Implements Command::parse()
 *
 * The 'subscribe-to' command allows to subscribe to an IMAP-Mailbox.
 * Patterns are not supported.
 * Use it as follows: subscribe-to =folder
 */
enum CommandResult parse_subscribe_to(struct Buffer *buf, struct Buffer *s,
                                      intptr_t data, struct Buffer *err)
{
  if (!buf || !s || !err)
    return MUTT_CMD_ERROR;

  mutt_buffer_reset(err);

  if (MoreArgs(s))
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

    if (MoreArgs(s))
    {
      mutt_buffer_printf(err, _("%s: too many arguments"), "subscribe-to");
      return MUTT_CMD_WARNING;
    }

    if (buf->data && (*buf->data != '\0'))
    {
      /* Expand and subscribe */
      if (imap_subscribe(mutt_expand_path(buf->data, buf->dsize), true) == 0)
      {
        mutt_message(_("Subscribed to %s"), buf->data);
        return MUTT_CMD_SUCCESS;
      }

      mutt_buffer_printf(err, _("Could not subscribe to %s"), buf->data);
      return MUTT_CMD_ERROR;
    }

    mutt_debug(LL_DEBUG1, "Corrupted buffer");
    return MUTT_CMD_ERROR;
  }

  mutt_buffer_addstr(err, _("No folder specified"));
  return MUTT_CMD_WARNING;
}
#endif

/**
 * parse_tag_formats - Parse the 'tag-formats' command - Implements Command::parse()
 */
enum CommandResult parse_tag_formats(struct Buffer *buf, struct Buffer *s,
                                     intptr_t data, struct Buffer *err)
{
  if (!buf || !s)
    return MUTT_CMD_ERROR;

  char *tmp = NULL;

  while (MoreArgs(s))
  {
    char *tag = NULL, *format = NULL;

    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    if (buf->data && (*buf->data != '\0'))
      tag = mutt_str_dup(buf->data);
    else
      continue;

    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    format = mutt_str_dup(buf->data);

    /* avoid duplicates */
    tmp = mutt_hash_find(TagFormats, format);
    if (tmp)
    {
      mutt_debug(LL_DEBUG3, "tag format '%s' already registered as '%s'\n", format, tmp);
      FREE(&tag);
      FREE(&format);
      continue;
    }

    mutt_hash_insert(TagFormats, format, tag);
  }
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_tag_transforms - Parse the 'tag-transforms' command - Implements Command::parse()
 */
enum CommandResult parse_tag_transforms(struct Buffer *buf, struct Buffer *s,
                                        intptr_t data, struct Buffer *err)
{
  if (!buf || !s)
    return MUTT_CMD_ERROR;

  char *tmp = NULL;

  while (MoreArgs(s))
  {
    char *tag = NULL, *transform = NULL;

    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    if (buf->data && (*buf->data != '\0'))
      tag = mutt_str_dup(buf->data);
    else
      continue;

    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    transform = mutt_str_dup(buf->data);

    /* avoid duplicates */
    tmp = mutt_hash_find(TagTransforms, tag);
    if (tmp)
    {
      mutt_debug(LL_DEBUG3, "tag transform '%s' already registered as '%s'\n", tag, tmp);
      FREE(&tag);
      FREE(&transform);
      continue;
    }

    mutt_hash_insert(TagTransforms, tag, transform);
  }
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_unignore - Parse the 'unignore' command - Implements Command::parse()
 */
enum CommandResult parse_unignore(struct Buffer *buf, struct Buffer *s,
                                  intptr_t data, struct Buffer *err)
{
  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

    /* don't add "*" to the unignore list */
    if (strcmp(buf->data, "*") != 0)
      add_to_stailq(&UnIgnore, buf->data);

    remove_from_stailq(&Ignore, buf->data);
  } while (MoreArgs(s));

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_unlists - Parse the 'unlists' command - Implements Command::parse()
 */
enum CommandResult parse_unlists(struct Buffer *buf, struct Buffer *s,
                                 intptr_t data, struct Buffer *err)
{
  mutt_hash_free(&AutoSubscribeCache);
  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    mutt_regexlist_remove(&SubscribedLists, buf->data);
    mutt_regexlist_remove(&MailLists, buf->data);

    if (!mutt_str_equal(buf->data, "*") &&
        (mutt_regexlist_add(&UnMailLists, buf->data, REG_ICASE, err) != 0))
    {
      return MUTT_CMD_ERROR;
    }
  } while (MoreArgs(s));

  return MUTT_CMD_SUCCESS;
}

/**
 * do_unmailboxes - Remove a Mailbox from the Sidebar/notifications
 * @param m Mailbox to `unmailboxes`
 */
static void do_unmailboxes(struct Mailbox *m)
{
#ifdef USE_INOTIFY
  mutt_monitor_remove(m);
#endif
  m->flags = MB_HIDDEN;
  m->gen = -1;
  if (m->opened)
  {
    struct EventMailbox ev_m = { NULL };
    mutt_debug(LL_NOTIFY, "NT_MAILBOX_SWITCH: NULL\n");
    notify_send(NeoMutt->notify, NT_MAILBOX, NT_MAILBOX_SWITCH, &ev_m);
  }
  else
  {
    account_mailbox_remove(m->account, m);
    mailbox_free(&m);
  }
}

/**
 * do_unmailboxes_star - Remove all Mailboxes from the Sidebar/notifications
 */
static void do_unmailboxes_star(void)
{
  struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
  neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
  struct MailboxNode *np = NULL;
  struct MailboxNode *nptmp = NULL;
  STAILQ_FOREACH_SAFE(np, &ml, entries, nptmp)
  {
    do_unmailboxes(np->mailbox);
  }
  neomutt_mailboxlist_clear(&ml);
}

/**
 * parse_unmailboxes - Parse the 'unmailboxes' command - Implements Command::parse()
 *
 * This is also used by 'unvirtual-mailboxes'
 */
enum CommandResult parse_unmailboxes(struct Buffer *buf, struct Buffer *s,
                                     intptr_t data, struct Buffer *err)
{
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  if (data & MUTT_COMMAND_DEPRECATED)
  {
    mutt_warning(_("Command '%s' is deprecated"), buf->data);
    rc = MUTT_CMD_WARNING;
  }

  while (MoreArgs(s))
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

    if (mutt_str_equal(buf->data, "*"))
    {
      do_unmailboxes_star();
      return rc;
    }

    mutt_buffer_expand_path(buf);

    struct Account *a = NULL;
    TAILQ_FOREACH(a, &NeoMutt->accounts, entries)
    {
      struct Mailbox *m = mx_mbox_find(a, mutt_buffer_string(buf));
      if (m)
      {
        do_unmailboxes(m);
        break;
      }
    }
  }
  return rc;
}

/**
 * parse_unmy_hdr - Parse the 'unmy_hdr' command - Implements Command::parse()
 */
enum CommandResult parse_unmy_hdr(struct Buffer *buf, struct Buffer *s,
                                  intptr_t data, struct Buffer *err)
{
  struct ListNode *np = NULL, *tmp = NULL;
  size_t l;

  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    if (mutt_str_equal("*", buf->data))
    {
      /* Clear all headers, send a notification for each header */
      STAILQ_FOREACH(np, &UserHeader, entries)
      {
        mutt_debug(LL_NOTIFY, "NT_HEADER_DELETE: %s\n", np->data);
        struct EventHeader ev_h = { np->data };
        notify_send(NeoMutt->notify, NT_HEADER, NT_HEADER_DELETE, &ev_h);
      }
      mutt_list_free(&UserHeader);
      continue;
    }

    l = mutt_str_len(buf->data);
    if (buf->data[l - 1] == ':')
      l--;

    STAILQ_FOREACH_SAFE(np, &UserHeader, entries, tmp)
    {
      if (mutt_istrn_equal(buf->data, np->data, l) && (np->data[l] == ':'))
      {
        mutt_debug(LL_NOTIFY, "NT_HEADER_DELETE: %s\n", np->data);
        struct EventHeader ev_h = { np->data };
        notify_send(NeoMutt->notify, NT_HEADER, NT_HEADER_DELETE, &ev_h);

        header_free(&UserHeader, np);
      }
    }
  } while (MoreArgs(s));
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_unstailq - Parse an unlist command - Implements Command::parse()
 *
 * This is used by 'unalternative_order', 'unauto_view' and several others.
 */
enum CommandResult parse_unstailq(struct Buffer *buf, struct Buffer *s,
                                  intptr_t data, struct Buffer *err)
{
  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    /* Check for deletion of entire list */
    if (mutt_str_equal(buf->data, "*"))
    {
      mutt_list_free((struct ListHead *) data);
      break;
    }
    remove_from_stailq((struct ListHead *) data, buf->data);
  } while (MoreArgs(s));

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_unsubscribe - Parse the 'unsubscribe' command - Implements Command::parse()
 */
enum CommandResult parse_unsubscribe(struct Buffer *buf, struct Buffer *s,
                                     intptr_t data, struct Buffer *err)
{
  mutt_hash_free(&AutoSubscribeCache);
  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    mutt_regexlist_remove(&SubscribedLists, buf->data);

    if (!mutt_str_equal(buf->data, "*") &&
        (mutt_regexlist_add(&UnSubscribedLists, buf->data, REG_ICASE, err) != 0))
    {
      return MUTT_CMD_ERROR;
    }
  } while (MoreArgs(s));

  return MUTT_CMD_SUCCESS;
}

#ifdef USE_IMAP
/**
 * parse_unsubscribe_from - Parse the 'unsubscribe-from' command - Implements Command::parse()
 *
 * The 'unsubscribe-from' command allows to unsubscribe from an IMAP-Mailbox.
 * Patterns are not supported.
 * Use it as follows: unsubscribe-from =folder
 */
enum CommandResult parse_unsubscribe_from(struct Buffer *buf, struct Buffer *s,
                                          intptr_t data, struct Buffer *err)
{
  if (!buf || !s || !err)
    return MUTT_CMD_ERROR;

  if (MoreArgs(s))
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

    if (MoreArgs(s))
    {
      mutt_buffer_printf(err, _("%s: too many arguments"), "unsubscribe-from");
      return MUTT_CMD_WARNING;
    }

    if (buf->data && (*buf->data != '\0'))
    {
      /* Expand and subscribe */
      if (imap_subscribe(mutt_expand_path(buf->data, buf->dsize), false) == 0)
      {
        mutt_message(_("Unsubscribed from %s"), buf->data);
        return MUTT_CMD_SUCCESS;
      }

      mutt_buffer_printf(err, _("Could not unsubscribe from %s"), buf->data);
      return MUTT_CMD_ERROR;
    }

    mutt_debug(LL_DEBUG1, "Corrupted buffer");
    return MUTT_CMD_ERROR;
  }

  mutt_buffer_addstr(err, _("No folder specified"));
  return MUTT_CMD_WARNING;
}
#endif

/**
 * clear_source_stack - Free memory from the stack used for the souce command
 */
void clear_source_stack(void)
{
  mutt_list_free(&MuttrcStack);
}
