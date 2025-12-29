/**
 * @file
 * Functions to parse commands in a config file
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2010,2012-2013,2016 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
 * Copyright (C) 2019-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Aditya De Saha <adityadesaha@gmail.com>
 * Copyright (C) 2020 Matthew Hughes <matthewhughes934@gmail.com>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
 * Copyright (C) 2020-2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2022 Marco Sirabella <marco@sirabella.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * @page neo_commands Functions to parse commands in a config file
 *
 * Functions to parse commands in a config file
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "commands.h"
#include "attach/lib.h"
#include "color/lib.h"
#include "imap/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "pager/lib.h"
#include "parse/lib.h"
#include "store/lib.h"
#include "alternates.h"
#include "globals.h"
#include "muttlib.h"
#include "mx.h"
#include "score.h"
#include "version.h"
#ifdef USE_INOTIFY
#include "monitor.h"
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

/// LIFO designed to contain the list of config files that have been sourced and
/// avoid cyclic sourcing.
static struct ListHead MuttrcStack = STAILQ_HEAD_INITIALIZER(MuttrcStack);

#define MAX_ERRS 128

/**
 * enum TriBool - Tri-state boolean
 */
enum TriBool
{
  TB_UNSET = -1, ///< Value hasn't been set
  TB_FALSE,      ///< Value is false
  TB_TRUE,       ///< Value is true
};

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
 * command_find_by_id - Find a NeoMutt Command by its CommandId
 * @param ca Array of Commands
 * @param id ID to find
 * @retval ptr Matching Command
 */
const struct Command *command_find_by_id(const struct CommandArray *ca, enum CommandId id)
{
  if (!ca || (id == CMD_NONE))
    return NULL;

  const struct Command **cp = NULL;
  ARRAY_FOREACH(cp, ca)
  {
    const struct Command *cmd = *cp;

    if (cmd->id == id)
      return cmd;
  }

  return NULL;
}

/**
 * command_find_by_name - Find a NeoMutt Command by its name
 * @param ca   Array of Commands
 * @param name Name to find
 * @retval ptr Matching Command
 */
const struct Command *command_find_by_name(const struct CommandArray *ca, const char *name)
{
  if (!ca || !name)
    return NULL;

  const struct Command **cp = NULL;
  ARRAY_FOREACH(cp, ca)
  {
    const struct Command *cmd = *cp;

    if (mutt_str_equal(cmd->name, name))
      return cmd;
  }

  return NULL;
}

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
    const struct MenuFuncOp *fns = km_get_table(MenuNames[i].value);
    if (!fns)
      continue;

    for (int j = 0; fns[j].name; j++)
      if (mutt_str_equal(name, fns[j].name))
        return true;
  }
  return false;
}

/**
 * is_color_object - Is the argument a neomutt colour?
 * @param name  Colour name to be searched for
 * @retval true  Function found
 * @retval false Function not found
 */
static bool is_color_object(const char *name)
{
  int cid = mutt_map_get_value(name, ColorFields);

  return (cid > 0);
}

/**
 * parse_grouplist - Parse a group context
 * @param gl     GroupList to add to
 * @param token  Temporary Buffer space
 * @param line   Buffer containing string to be parsed
 * @param err    Buffer for error messages
 * @param groups Groups HashTable
 * @retval  0 Success
 * @retval -1 Error
 */
int parse_grouplist(struct GroupList *gl, struct Buffer *token, struct Buffer *line,
                    struct Buffer *err, struct HashTable *groups)
{
  while (mutt_istr_equal(token->data, "-group"))
  {
    if (!MoreArgs(line))
    {
      buf_strcpy(err, _("-group: no group name"));
      return -1;
    }

    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    grouplist_add_group(gl, groups_get_group(groups, token->data));

    if (!MoreArgs(line))
    {
      buf_strcpy(err, _("out of arguments"));
      return -1;
    }

    parse_extract_token(token, line, TOKEN_NO_FLAGS);
  }

  return 0;
}

/**
 * parse_rc_line_cwd - Parse and run a muttrc line in a relative directory
 * @param line   Line to be parsed
 * @param cwd    File relative where to run the line
 * @param err    Where to write error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
enum CommandResult parse_rc_line_cwd(const char *line, char *cwd, struct Buffer *err)
{
  mutt_list_insert_head(&MuttrcStack, mutt_str_dup(NONULL(cwd)));

  struct Buffer *buf = buf_pool_get();
  buf_strcpy(buf, line);
  enum CommandResult ret = parse_rc_line(buf, err);
  buf_pool_release(&buf);

  struct ListNode *np = STAILQ_FIRST(&MuttrcStack);
  STAILQ_REMOVE_HEAD(&MuttrcStack, entries);
  FREE(&np->data);
  FREE(&np);

  return ret;
}

/**
 * mutt_get_sourced_cwd - Get the current file path that is being parsed
 * @retval ptr File path that is being parsed or cwd at runtime
 *
 * @note Caller is responsible for freeing returned string
 */
char *mutt_get_sourced_cwd(void)
{
  struct ListNode *np = STAILQ_FIRST(&MuttrcStack);
  if (np && np->data)
    return mutt_str_dup(np->data);

  // stack is empty, return our own dummy file relative to cwd
  struct Buffer *cwd = buf_pool_get();
  mutt_path_getcwd(cwd);
  buf_addstr(cwd, "/dummy.rc");
  char *ret = buf_strdup(cwd);
  buf_pool_release(&cwd);
  return ret;
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
  struct Buffer *linebuf = NULL;
  char *line = NULL;
  char *currentline = NULL;
  char rcfile[PATH_MAX + 1] = { 0 };
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
    buf_printf(err, "%s: %s", rcfile, strerror(errno));
    return -1;
  }

  linebuf = buf_pool_get();

  const char *const c_config_charset = cs_subset_string(NeoMutt->sub, "config_charset");
  const char *const c_charset = cc_charset();
  while ((line = mutt_file_read_line(line, &linelen, fp, &lineno, MUTT_RL_CONT)) != NULL)
  {
    const bool conv = c_config_charset && c_charset;
    if (conv)
    {
      currentline = mutt_str_dup(line);
      if (!currentline)
        continue;
      mutt_ch_convert_string(&currentline, c_config_charset, c_charset, MUTT_ICONV_NO_FLAGS);
    }
    else
    {
      currentline = line;
    }

    buf_strcpy(linebuf, currentline);

    buf_reset(err);
    line_rc = parse_rc_line(linebuf, err);
    if (line_rc == MUTT_CMD_ERROR)
    {
      mutt_error("%s:%d: %s", rcfile, lineno, buf_string(err));
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
      mutt_warning("%s:%d: %s", rcfile, lineno, buf_string(err));
      warnings++;
    }
    else if (line_rc == MUTT_CMD_FINISH)
    {
      if (conv)
        FREE(&currentline);
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
    buf_reset(err);
    buf_printf(err, (rc >= -MAX_ERRS) ? _("source: errors in %s") : _("source: reading aborted due to too many errors in %s"),
               rcfile);
    rc = -1;
  }
  else
  {
    /* Don't alias errors with warnings */
    if (warnings > 0)
    {
      buf_printf(err, ngettext("source: %d warning in %s", "source: %d warnings in %s", warnings),
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

  buf_pool_release(&linebuf);
  return rc;
}

/**
 * parse_cd - Parse the 'cd' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `cd [ <directory> ]`
 */
enum CommandResult parse_cd(const struct Command *cmd, struct Buffer *line, struct Buffer *err)
{
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  parse_extract_token(token, line, TOKEN_NO_FLAGS);
  if (buf_is_empty(token))
  {
    buf_strcpy(token, NeoMutt->home_dir);
  }
  else
  {
    buf_expand_path(token);
  }

  if (chdir(buf_string(token)) != 0)
  {
    buf_printf(err, "%s: %s", cmd->name, strerror(errno));
    goto done;
  }

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_echo - Parse the 'echo' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `echo <message>`
 */
enum CommandResult parse_echo(const struct Command *cmd, struct Buffer *line,
                              struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  parse_extract_token(token, line, TOKEN_NO_FLAGS);
  OptForceRefresh = true;
  mutt_message("%s", buf_string(token));
  OptForceRefresh = false;
  mutt_sleep(0);

  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_finish - Parse the 'finish' command - Implements Command::parse() - @ingroup command_parse
 * @retval  #MUTT_CMD_FINISH Stop processing the current file
 * @retval  #MUTT_CMD_WARNING Failed
 *
 * If the 'finish' command is found, we should stop reading the current file.
 *
 * Parse:
 * - `finish`
 */
enum CommandResult parse_finish(const struct Command *cmd, struct Buffer *line,
                                struct Buffer *err)
{
  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  return MUTT_CMD_FINISH;
}

/**
 * parse_group - Parse the 'group' and 'ungroup' commands - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `group [ -group <name> ... ] { -rx <regex> ... | -addr <address> ... }`
 * - `ungroup [ -group <name> ... ] { * | -rx <regex> ... | -addr <address> ... }`
 */
enum CommandResult parse_group(const struct Command *cmd, struct Buffer *line,
                               struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct GroupList gl = STAILQ_HEAD_INITIALIZER(gl);
  enum GroupState gstate = GS_NONE;
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    if (parse_grouplist(&gl, token, line, err, NeoMutt->groups) == -1)
      goto done;

    if ((cmd->id == CMD_UNGROUP) && mutt_istr_equal(buf_string(token), "*"))
    {
      groups_remove_grouplist(NeoMutt->groups, &gl);
      rc = MUTT_CMD_SUCCESS;
      goto done;
    }

    if (mutt_istr_equal(buf_string(token), "-rx"))
    {
      gstate = GS_RX;
    }
    else if (mutt_istr_equal(buf_string(token), "-addr"))
    {
      gstate = GS_ADDR;
    }
    else
    {
      switch (gstate)
      {
        case GS_NONE:
          buf_printf(err, _("%s: missing -rx or -addr"), cmd->name);
          rc = MUTT_CMD_WARNING;
          goto done;

        case GS_RX:
          if ((cmd->id == CMD_GROUP) &&
              (grouplist_add_regex(&gl, buf_string(token), REG_ICASE, err) != 0))
          {
            goto done;
          }
          else if ((cmd->id == CMD_UNGROUP) &&
                   (groups_remove_regex(NeoMutt->groups, &gl, buf_string(token)) < 0))
          {
            goto done;
          }
          break;

        case GS_ADDR:
        {
          char *estr = NULL;
          struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
          mutt_addrlist_parse2(&al, buf_string(token));
          if (TAILQ_EMPTY(&al))
            goto done;
          if (mutt_addrlist_to_intl(&al, &estr))
          {
            buf_printf(err, _("%s: warning: bad IDN '%s'"), cmd->name, estr);
            mutt_addrlist_clear(&al);
            FREE(&estr);
            goto done;
          }
          if (cmd->id == CMD_GROUP)
            grouplist_add_addrlist(&gl, &al);
          else if (cmd->id == CMD_UNGROUP)
            groups_remove_addrlist(NeoMutt->groups, &gl, &al);
          mutt_addrlist_clear(&al);
          break;
        }
      }
    }
  } while (MoreArgs(line));

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  grouplist_destroy(&gl);
  return rc;
}

/**
 * parse_ifdef - Parse the 'ifdef' and 'ifndef' commands - Implements Command::parse() - @ingroup command_parse
 *
 * The 'ifdef' command allows conditional elements in the config file.
 * If a given variable, function, command or compile-time symbol exists, then
 * read the rest of the line of config commands.
 * e.g.
 *      ifdef sidebar source ~/.neomutt/sidebar.rc
 *
 * If (cmd->data == 1) then it means use the 'ifndef' (if-not-defined) command.
 * e.g.
 *      ifndef imap finish
 *
 * Parse:
 * - `ifdef <symbol> "<config-command> [ <args> ... ]"`
 * - `ifndef <symbol> "<config-command> [ <args> ... ]"`
 */
enum CommandResult parse_ifdef(const struct Command *cmd, struct Buffer *line,
                               struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  parse_extract_token(token, line, TOKEN_NO_FLAGS);

  // is the item defined as:
  bool res = cs_subset_lookup(NeoMutt->sub, buf_string(token)) // a variable?
             || feature_enabled(buf_string(token)) // a compiled-in feature?
             || is_function(buf_string(token))     // a function?
             || commands_get(&NeoMutt->commands, buf_string(token)) // a command?
             || is_color_object(buf_string(token))                  // a color?
#ifdef USE_HCACHE
             || store_is_valid_backend(buf_string(token)) // a store? (database)
#endif
             || mutt_str_getenv(buf_string(token)); // an environment variable?

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto done;
  }
  parse_extract_token(token, line, TOKEN_SPACE);

  /* ifdef KNOWN_SYMBOL or ifndef UNKNOWN_SYMBOL */
  if ((res && (cmd->id == CMD_IFDEF)) || (!res && (cmd->id == CMD_IFNDEF)))
  {
    rc = parse_rc_line(token, err);
    if (rc == MUTT_CMD_ERROR)
      mutt_error(_("Error: %s"), buf_string(err));

    goto done;
  }

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_ignore - Parse the 'ignore' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `ignore { * | <string> ... }`
 */
enum CommandResult parse_ignore(const struct Command *cmd, struct Buffer *line,
                                struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    remove_from_stailq(&UnIgnore, buf_string(token));
    add_to_stailq(&Ignore, buf_string(token));
  } while (MoreArgs(line));

  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_lists - Parse the 'lists' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `lists [ -group <name> ... ] <regex> [ <regex> ... ]`
 */
enum CommandResult parse_lists(const struct Command *cmd, struct Buffer *line,
                               struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct GroupList gl = STAILQ_HEAD_INITIALIZER(gl);
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    if (parse_grouplist(&gl, token, line, err, NeoMutt->groups) == -1)
      goto done;

    mutt_regexlist_remove(&UnMailLists, buf_string(token));

    if (mutt_regexlist_add(&MailLists, buf_string(token), REG_ICASE, err) != 0)
      goto done;

    if (grouplist_add_regex(&gl, buf_string(token), REG_ICASE, err) != 0)
      goto done;
  } while (MoreArgs(line));

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  grouplist_destroy(&gl);
  return rc;
}

/**
 * mailbox_add - Add a new Mailbox
 * @param folder  Path to use for '+' abbreviations
 * @param mailbox Mailbox to add
 * @param label   Descriptive label
 * @param poll    Enable mailbox polling?
 * @param notify  Enable mailbox notification?
 * @param err     Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
static enum CommandResult mailbox_add(const char *folder, const char *mailbox,
                                      const char *label, enum TriBool poll,
                                      enum TriBool notify, struct Buffer *err)
{
  mutt_debug(LL_DEBUG1, "Adding mailbox: '%s' label '%s', poll %s, notify %s\n",
             mailbox, label ? label : "[NONE]",
             (poll == TB_UNSET) ? "[UNSPECIFIED]" :
             (poll == TB_TRUE)  ? "true" :
                                  "false",
             (notify == TB_UNSET) ? "[UNSPECIFIED]" :
             (notify == TB_TRUE)  ? "true" :
                                    "false");
  struct Mailbox *m = mailbox_new();

  buf_strcpy(&m->pathbuf, mailbox);
  /* int rc = */ mx_path_canon2(m, folder);

  if (m->type <= MUTT_UNKNOWN)
  {
    buf_printf(err, "Unknown Mailbox: %s", m->realpath);
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
      if (!m_old->visible)
      {
        m_old->visible = true;
        m_old->gen = mailbox_gen();
      }

      if (label)
        mutt_str_replace(&m_old->name, label);

      if (notify != TB_UNSET)
        m_old->notify_user = notify;

      if (poll != TB_UNSET)
        m_old->poll_new_mail = poll;

      struct EventMailbox ev_m = { m_old };
      notify_send(m_old->notify, NT_MAILBOX, NT_MAILBOX_CHANGE, &ev_m);

      mailbox_free(&m);
      return MUTT_CMD_SUCCESS;
    }
  }

  if (label)
    m->name = mutt_str_dup(label);

  if (notify != TB_UNSET)
    m->notify_user = notify;

  if (poll != TB_UNSET)
    m->poll_new_mail = poll;

  if (!mx_ac_add(a, m))
  {
    mailbox_free(&m);
    if (new_account)
    {
      cs_subset_free(&a->sub);
      FREE(&a->name);
      notify_free(&a->notify);
      FREE(&a);
    }
    return MUTT_CMD_SUCCESS;
  }

  if (new_account)
  {
    neomutt_account_add(NeoMutt, a);
  }

  // this is finally a visible mailbox in the sidebar and mailboxes list
  m->visible = true;

#ifdef USE_INOTIFY
  mutt_monitor_add(m);
#endif

  return MUTT_CMD_SUCCESS;
}

/**
 * mailbox_add_simple - Add a new Mailbox
 * @param mailbox Mailbox to add
 * @param err     Buffer for error messages
 * @retval true Success
 */
bool mailbox_add_simple(const char *mailbox, struct Buffer *err)
{
  enum CommandResult rc = mailbox_add("", mailbox, NULL, TB_UNSET, TB_UNSET, err);

  return (rc == MUTT_CMD_SUCCESS);
}

/**
 * parse_mailboxes - Parse the 'mailboxes' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `mailboxes [[ -label <label> ] | -nolabel ] [[ -notify | -nonotify ] [ -poll | -nopoll ] <mailbox> ] [ ... ]`
 * - `named-mailboxes <description> <mailbox> [ <description> <mailbox> ... ]`
 */
enum CommandResult parse_mailboxes(const struct Command *cmd,
                                   struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *label = buf_pool_get();
  struct Buffer *mailbox = buf_pool_get();
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_WARNING;

  const char *const c_folder = cs_subset_string(NeoMutt->sub, "folder");
  while (MoreArgs(line))
  {
    bool label_set = false;
    enum TriBool notify = TB_UNSET;
    enum TriBool poll = TB_UNSET;

    do
    {
      // Start by handling the options
      parse_extract_token(token, line, TOKEN_NO_FLAGS);

      if (mutt_str_equal(buf_string(token), "-label"))
      {
        if (!MoreArgs(line))
        {
          buf_printf(err, _("%s: too few arguments"), "mailboxes -label");
          goto done;
        }

        parse_extract_token(label, line, TOKEN_NO_FLAGS);
        label_set = true;
      }
      else if (mutt_str_equal(buf_string(token), "-nolabel"))
      {
        buf_reset(label);
        label_set = true;
      }
      else if (mutt_str_equal(buf_string(token), "-notify"))
      {
        notify = TB_TRUE;
      }
      else if (mutt_str_equal(buf_string(token), "-nonotify"))
      {
        notify = TB_FALSE;
      }
      else if (mutt_str_equal(buf_string(token), "-poll"))
      {
        poll = TB_TRUE;
      }
      else if (mutt_str_equal(buf_string(token), "-nopoll"))
      {
        poll = TB_FALSE;
      }
      else if ((cmd->id == CMD_NAMED_MAILBOXES) && !label_set)
      {
        if (!MoreArgs(line))
        {
          buf_printf(err, _("%s: too few arguments"), cmd->name);
          goto done;
        }

        buf_copy(label, token);
        label_set = true;
      }
      else
      {
        buf_copy(mailbox, token);
        break;
      }
    } while (MoreArgs(line));

    if (buf_is_empty(mailbox))
    {
      buf_printf(err, _("%s: too few arguments"), cmd->name);
      goto done;
    }

    rc = mailbox_add(c_folder, buf_string(mailbox),
                     label_set ? buf_string(label) : NULL, poll, notify, err);
    if (rc != MUTT_CMD_SUCCESS)
      goto done;

    buf_reset(label);
    buf_reset(mailbox);
  }

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&label);
  buf_pool_release(&mailbox);
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_my_hdr - Parse the 'my_hdr' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `my_hdr <string>`
 */
enum CommandResult parse_my_hdr(const struct Command *cmd, struct Buffer *line,
                                struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_WARNING;

  parse_extract_token(token, line, TOKEN_SPACE | TOKEN_QUOTE);
  char *p = strpbrk(buf_string(token), ": \t");
  if (!p || (*p != ':'))
  {
    buf_strcpy(err, _("invalid header field"));
    goto done;
  }

  struct EventHeader ev_h = { token->data };
  struct ListNode *node = header_find(&UserHeader, buf_string(token));

  if (node)
  {
    header_update(node, buf_string(token));
    mutt_debug(LL_NOTIFY, "NT_HEADER_CHANGE: %s\n", buf_string(token));
    notify_send(NeoMutt->notify, NT_HEADER, NT_HEADER_CHANGE, &ev_h);
  }
  else
  {
    header_add(&UserHeader, buf_string(token));
    mutt_debug(LL_NOTIFY, "NT_HEADER_ADD: %s\n", buf_string(token));
    notify_send(NeoMutt->notify, NT_HEADER, NT_HEADER_ADD, &ev_h);
  }

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * set_dump - Dump list of config variables into a file/pager
 * @param flags Which config to dump, e.g. #GEL_CHANGED_CONFIG
 * @param err   Buffer for error message
 * @return num See #CommandResult
 *
 * FIXME: Move me into parse/set.c.  Note: this function currently depends on
 * pager, which is the reason it is not included in the parse library.
 */
enum CommandResult set_dump(enum GetElemListFlags flags, struct Buffer *err)
{
  struct Buffer *tempfile = buf_pool_get();
  buf_mktemp(tempfile);

  FILE *fp_out = mutt_file_fopen(buf_string(tempfile), "w");
  if (!fp_out)
  {
    // L10N: '%s' is the file name of the temporary file
    buf_printf(err, _("Could not create temporary file %s"), buf_string(tempfile));
    buf_pool_release(&tempfile);
    return MUTT_CMD_ERROR;
  }

  struct ConfigSet *cs = NeoMutt->sub->cs;
  struct HashElemArray hea = get_elem_list(cs, flags);
  dump_config(cs, &hea, CS_DUMP_NO_FLAGS, fp_out);
  ARRAY_FREE(&hea);

  mutt_file_fclose(&fp_out);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(tempfile);

  pview.banner = "set";
  pview.flags = MUTT_PAGER_NO_FLAGS;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  buf_pool_release(&tempfile);

  return MUTT_CMD_SUCCESS;
}

/**
 * envlist_sort - Compare two environment strings - Implements ::sort_t - @ingroup sort_api
 */
static int envlist_sort(const void *a, const void *b, void *sdata)
{
  return strcmp(*(const char **) a, *(const char **) b);
}

/**
 * parse_setenv - Parse the 'setenv' and 'unsetenv' commands - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `setenv { <variable>? | <variable> <value> }`
 * - `unsetenv <variable>`
 */
enum CommandResult parse_setenv(const struct Command *cmd, struct Buffer *line,
                                struct Buffer *err)
{
  struct Buffer *token = buf_pool_get();
  struct Buffer *tempfile = NULL;
  enum CommandResult rc = MUTT_CMD_WARNING;

  char **envp = NeoMutt->env;

  bool query = false;
  bool prefix = false;
  bool unset = (cmd->id == CMD_UNSETENV);

  if (!MoreArgs(line))
  {
    if (!StartupComplete)
    {
      buf_printf(err, _("%s: too few arguments"), cmd->name);
      goto done;
    }

    tempfile = buf_pool_get();
    buf_mktemp(tempfile);

    FILE *fp_out = mutt_file_fopen(buf_string(tempfile), "w");
    if (!fp_out)
    {
      // L10N: '%s' is the file name of the temporary file
      buf_printf(err, _("Could not create temporary file %s"), buf_string(tempfile));
      rc = MUTT_CMD_ERROR;
      goto done;
    }

    int count = 0;
    for (char **env = NeoMutt->env; *env; env++)
      count++;

    mutt_qsort_r(NeoMutt->env, count, sizeof(char *), envlist_sort, NULL);

    for (char **env = NeoMutt->env; *env; env++)
      fprintf(fp_out, "%s\n", *env);

    mutt_file_fclose(&fp_out);

    struct PagerData pdata = { 0 };
    struct PagerView pview = { &pdata };

    pdata.fname = buf_string(tempfile);

    pview.banner = cmd->name;
    pview.flags = MUTT_PAGER_NO_FLAGS;
    pview.mode = PAGER_MODE_OTHER;

    mutt_do_pager(&pview, NULL);

    rc = MUTT_CMD_SUCCESS;
    goto done;
  }

  if (*line->dptr == '?')
  {
    query = true;
    prefix = true;

    if (unset)
    {
      buf_printf(err, _("Can't query option with the '%s' command"), cmd->name);
      goto done;
    }

    line->dptr++;
  }

  /* get variable name */
  parse_extract_token(token, line, TOKEN_EQUAL | TOKEN_QUESTION);

  if (*line->dptr == '?')
  {
    if (unset)
    {
      buf_printf(err, _("Can't query option with the '%s' command"), cmd->name);
      goto done;
    }

    if (prefix)
    {
      buf_printf(err, _("Can't use a prefix when querying a variable"));
      goto done;
    }

    query = true;
    line->dptr++;
  }

  if (query)
  {
    bool found = false;
    while (envp && *envp)
    {
      /* This will display all matches for "^QUERY" */
      if (mutt_str_startswith(*envp, buf_string(token)))
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
      rc = MUTT_CMD_SUCCESS;
      goto done;
    }

    buf_printf(err, _("%s is unset"), buf_string(token));
    goto done;
  }

  if (unset)
  {
    if (envlist_unset(&NeoMutt->env, buf_string(token)))
      rc = MUTT_CMD_SUCCESS;
    else
      buf_printf(err, _("%s is unset"), buf_string(token));

    goto done;
  }

  /* set variable */

  if (*line->dptr == '=')
  {
    line->dptr++;
    SKIPWS(line->dptr);
  }

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    goto done;
  }

  char *varname = mutt_str_dup(buf_string(token));
  parse_extract_token(token, line, TOKEN_NO_FLAGS);
  envlist_set(&NeoMutt->env, varname, buf_string(token), true);
  FREE(&varname);

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  buf_pool_release(&tempfile);
  return rc;
}

/**
 * parse_source - Parse the 'source' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `source <filename>`
 */
enum CommandResult parse_source(const struct Command *cmd, struct Buffer *line,
                                struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  struct Buffer *path = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  do
  {
    if (parse_extract_token(token, line, TOKEN_BACKTICK_VARS) != 0)
    {
      buf_printf(err, _("source: error at %s"), line->dptr);
      goto done;
    }
    buf_copy(path, token);
    buf_expand_path(path);

    if (source_rc(buf_string(path), err) < 0)
    {
      buf_printf(err, _("source: file %s could not be sourced"), buf_string(path));
      goto done;
    }

  } while (MoreArgs(line));

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&path);
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_nospam - Parse the 'nospam' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `nospam { * | <regex> }`
 */
enum CommandResult parse_nospam(const struct Command *cmd, struct Buffer *line,
                                struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_WARNING;

  // Extract the first token, a regex or "*"
  parse_extract_token(token, line, TOKEN_NO_FLAGS);

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    goto done;
  }

  // "*" is special - clear both spam and nospam lists
  if (mutt_str_equal(buf_string(token), "*"))
  {
    mutt_replacelist_free(&SpamList);
    mutt_regexlist_free(&NoSpamList);
    rc = MUTT_CMD_SUCCESS;
    goto done;
  }

  // If it's on the spam list, just remove it
  if (mutt_replacelist_remove(&SpamList, buf_string(token)) != 0)
  {
    rc = MUTT_CMD_SUCCESS;
    goto done;
  }

  // Otherwise, add it to the nospam list
  if (mutt_regexlist_add(&NoSpamList, buf_string(token), REG_ICASE, err) != 0)
  {
    rc = MUTT_CMD_ERROR;
    goto done;
  }

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_spam - Parse the 'spam' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `spam <regex> <format>`
 */
enum CommandResult parse_spam(const struct Command *cmd, struct Buffer *line,
                              struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  struct Buffer *templ = NULL;
  enum CommandResult rc = MUTT_CMD_ERROR;

  // Extract the first token, a regex
  parse_extract_token(token, line, TOKEN_NO_FLAGS);

  // If there's a second parameter, it's a template for the spam tag
  if (MoreArgs(line))
  {
    templ = buf_pool_get();
    parse_extract_token(templ, line, TOKEN_NO_FLAGS);

    // Add to the spam list
    if (mutt_replacelist_add(&SpamList, buf_string(token), buf_string(templ), err) != 0)
      goto done;
  }
  else
  {
    // If not, try to remove from the nospam list
    mutt_regexlist_remove(&NoSpamList, buf_string(token));
  }

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&templ);
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_stailq - Parse a list command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `alternative_order <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]`
 * - `auto_view <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]`
 * - `hdr_order <header> [ <header> ... ]`
 * - `mailto_allow { * | <header-field> ... }`
 * - `mime_lookup <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]`
 */
enum CommandResult parse_stailq(const struct Command *cmd, struct Buffer *line,
                                struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    add_to_stailq((struct ListHead *) cmd->data, buf_string(token));
  } while (MoreArgs(line));

  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_subscribe - Parse the 'subscribe' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `subscribe [ -group <name> ... ] <regex> [ <regex> ... ]`
 */
enum CommandResult parse_subscribe(const struct Command *cmd,
                                   struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct GroupList gl = STAILQ_HEAD_INITIALIZER(gl);
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    if (parse_grouplist(&gl, token, line, err, NeoMutt->groups) == -1)
      goto done;

    mutt_regexlist_remove(&UnMailLists, buf_string(token));
    mutt_regexlist_remove(&UnSubscribedLists, buf_string(token));

    if (mutt_regexlist_add(&MailLists, buf_string(token), REG_ICASE, err) != 0)
      goto done;

    if (mutt_regexlist_add(&SubscribedLists, buf_string(token), REG_ICASE, err) != 0)
      goto done;

    if (grouplist_add_regex(&gl, buf_string(token), REG_ICASE, err) != 0)
      goto done;
  } while (MoreArgs(line));

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  grouplist_destroy(&gl);
  return rc;
}

/**
 * parse_subscribe_to - Parse the 'subscribe-to' command - Implements Command::parse() - @ingroup command_parse
 *
 * The 'subscribe-to' command allows to subscribe to an IMAP-Mailbox.
 * Patterns are not supported.
 *
 * Parse:
 * - `subscribe-to <imap-folder-uri>`
 */
enum CommandResult parse_subscribe_to(const struct Command *cmd,
                                      struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_WARNING;

  buf_reset(err);

  parse_extract_token(token, line, TOKEN_NO_FLAGS);

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    goto done;
  }

  // Expand and subscribe
  buf_expand_path(token);
  if (imap_subscribe(buf_string(token), true) != 0)
  {
    buf_printf(err, _("Could not subscribe to %s"), buf_string(token));
    rc = MUTT_CMD_ERROR;
    goto done;
  }

  mutt_message(_("Subscribed to %s"), buf_string(token));
  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_tag_formats - Parse the 'tag-formats' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse config like: `tag-formats pgp GP`
 *
 * @note This maps format -> tag
 *
 * Parse:
 * - `tag-formats <tag> <format-string> { tag format-string ...  }`
 */
enum CommandResult parse_tag_formats(const struct Command *cmd,
                                     struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *tag = buf_pool_get();
  struct Buffer *fmt = buf_pool_get();

  while (MoreArgs(line))
  {
    parse_extract_token(tag, line, TOKEN_NO_FLAGS);
    if (buf_is_empty(tag))
      continue;

    parse_extract_token(fmt, line, TOKEN_NO_FLAGS);

    /* avoid duplicates */
    const char *tmp = mutt_hash_find(TagFormats, buf_string(fmt));
    if (tmp)
    {
      mutt_warning(_("tag format '%s' already registered as '%s'"), buf_string(fmt), tmp);
      continue;
    }

    mutt_hash_insert(TagFormats, buf_string(fmt), buf_strdup(tag));
  }

  buf_pool_release(&tag);
  buf_pool_release(&fmt);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_tag_transforms - Parse the 'tag-transforms' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse config like: `tag-transforms pgp P`
 *
 * @note This maps tag -> transform
 *
 * Parse:
 * - `tag-transforms <tag> <transformed-string> { tag transformed-string ... }`
 */
enum CommandResult parse_tag_transforms(const struct Command *cmd,
                                        struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *tag = buf_pool_get();
  struct Buffer *trans = buf_pool_get();

  while (MoreArgs(line))
  {
    parse_extract_token(tag, line, TOKEN_NO_FLAGS);
    if (buf_is_empty(tag))
      continue;

    parse_extract_token(trans, line, TOKEN_NO_FLAGS);
    const char *trn = buf_string(trans);

    /* avoid duplicates */
    const char *tmp = mutt_hash_find(TagTransforms, buf_string(tag));
    if (tmp)
    {
      mutt_warning(_("tag transform '%s' already registered as '%s'"),
                   buf_string(tag), tmp);
      continue;
    }

    mutt_hash_insert(TagTransforms, buf_string(tag), mutt_str_dup(trn));
  }

  buf_pool_release(&tag);
  buf_pool_release(&trans);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_unignore - Parse the 'unignore' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unignore { * | <string> ... }`
 */
enum CommandResult parse_unignore(const struct Command *cmd,
                                  struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    /* don't add "*" to the unignore list */
    if (!mutt_str_equal(buf_string(token), "*"))
      add_to_stailq(&UnIgnore, buf_string(token));

    remove_from_stailq(&Ignore, buf_string(token));
  } while (MoreArgs(line));

  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_unlists - Parse the 'unlists' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unlists [ -group <name> ... ] { * | <regex> ... }`
 */
enum CommandResult parse_unlists(const struct Command *cmd, struct Buffer *line,
                                 struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  mutt_hash_free(&AutoSubscribeCache);
  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    mutt_regexlist_remove(&SubscribedLists, buf_string(token));
    mutt_regexlist_remove(&MailLists, buf_string(token));

    if (!mutt_str_equal(buf_string(token), "*") &&
        (mutt_regexlist_add(&UnMailLists, buf_string(token), REG_ICASE, err) != 0))
    {
      goto done;
    }
  } while (MoreArgs(line));

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * do_unmailboxes - Remove a Mailbox from the Sidebar/notifications
 * @param m Mailbox to `unmailboxes`
 */
static void do_unmailboxes(struct Mailbox *m)
{
#ifdef USE_INOTIFY
  if (m->poll_new_mail)
    mutt_monitor_remove(m);
#endif
  m->visible = false;
  m->gen = -1;
  if (m->opened)
  {
    struct EventMailbox ev_m = { NULL };
    mutt_debug(LL_NOTIFY, "NT_MAILBOX_CHANGE: NULL\n");
    notify_send(NeoMutt->notify, NT_MAILBOX, NT_MAILBOX_CHANGE, &ev_m);
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
  struct MailboxArray ma = neomutt_mailboxes_get(NeoMutt, MUTT_MAILBOX_ANY);

  struct Mailbox **mp = NULL;
  ARRAY_FOREACH(mp, &ma)
  {
    do_unmailboxes(*mp);
  }
  ARRAY_FREE(&ma); // Clean up the ARRAY, but not the Mailboxes
}

/**
 * parse_unmailboxes - Parse the 'unmailboxes' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unmailboxes { * | <mailbox> ... }`
 */
enum CommandResult parse_unmailboxes(const struct Command *cmd,
                                     struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  while (MoreArgs(line))
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    if (mutt_str_equal(buf_string(token), "*"))
    {
      do_unmailboxes_star();
      goto done;
    }

    buf_expand_path(token);

    struct Account **ap = NULL;
    ARRAY_FOREACH(ap, &NeoMutt->accounts)
    {
      struct Mailbox *m = mx_mbox_find(*ap, buf_string(token));
      if (m)
      {
        do_unmailboxes(m);
        break;
      }
    }
  }

done:
  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_unmy_hdr - Parse the 'unmy_hdr' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unmy_hdr { * | <field> ... }`
 */
enum CommandResult parse_unmy_hdr(const struct Command *cmd,
                                  struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  struct ListNode *np = NULL, *tmp = NULL;
  size_t l;

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    if (mutt_str_equal("*", buf_string(token)))
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

    l = mutt_str_len(buf_string(token));
    if (buf_at(token, l - 1) == ':')
      l--;

    STAILQ_FOREACH_SAFE(np, &UserHeader, entries, tmp)
    {
      if (mutt_istrn_equal(buf_string(token), np->data, l) && (np->data[l] == ':'))
      {
        mutt_debug(LL_NOTIFY, "NT_HEADER_DELETE: %s\n", np->data);
        struct EventHeader ev_h = { np->data };
        notify_send(NeoMutt->notify, NT_HEADER, NT_HEADER_DELETE, &ev_h);

        header_free(&UserHeader, np);
      }
    }
  } while (MoreArgs(line));
  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_unstailq - Parse an unlist command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unalternative_order { * | [ <mime-type>[/<mime-subtype> ] ... ] }`
 * - `unauto_view { * | [ <mime-type>[/<mime-subtype> ] ... ] }`
 * - `unhdr_order { * | <header> ... }`
 * - `unmailto_allow { * | <header-field> ... }`
 * - `unmime_lookup { * | [ <mime-type>[/<mime-subtype> ] ... ] }`
 */
enum CommandResult parse_unstailq(const struct Command *cmd,
                                  struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    /* Check for deletion of entire list */
    if (mutt_str_equal(buf_string(token), "*"))
    {
      mutt_list_free((struct ListHead *) cmd->data);
      break;
    }
    remove_from_stailq((struct ListHead *) cmd->data, buf_string(token));
  } while (MoreArgs(line));

  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_unsubscribe - Parse the 'unsubscribe' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unsubscribe [ -group <name> ... ] { * | <regex> ... }`
 */
enum CommandResult parse_unsubscribe(const struct Command *cmd,
                                     struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  mutt_hash_free(&AutoSubscribeCache);
  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    mutt_regexlist_remove(&SubscribedLists, buf_string(token));

    if (!mutt_str_equal(buf_string(token), "*") &&
        (mutt_regexlist_add(&UnSubscribedLists, buf_string(token), REG_ICASE, err) != 0))
    {
      goto done;
    }
  } while (MoreArgs(line));

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_unsubscribe_from - Parse the 'unsubscribe-from' command - Implements Command::parse() - @ingroup command_parse
 *
 * The 'unsubscribe-from' command allows to unsubscribe from an IMAP-Mailbox.
 * Patterns are not supported.
 *
 * Parse:
 * - `unsubscribe-from <imap-folder-uri>`
 */
enum CommandResult parse_unsubscribe_from(const struct Command *cmd,
                                          struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_WARNING;

  parse_extract_token(token, line, TOKEN_NO_FLAGS);

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    goto done;
  }

  // Expand and unsubscribe
  buf_expand_path(token);
  if (imap_subscribe(buf_string(token), false) != 0)
  {
    buf_printf(err, _("Could not unsubscribe from %s"), buf_string(token));
    rc = MUTT_CMD_ERROR;
    goto done;
  }

  mutt_message(_("Unsubscribed from %s"), buf_string(token));
  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_version - Parse the 'version' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `version`
 */
enum CommandResult parse_version(const struct Command *cmd, struct Buffer *line,
                                 struct Buffer *err)
{
  // silently ignore 'version' if it's in a config file
  if (!StartupComplete)
    return MUTT_CMD_SUCCESS;

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *tempfile = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  buf_mktemp(tempfile);

  FILE *fp_out = mutt_file_fopen(buf_string(tempfile), "w");
  if (!fp_out)
  {
    // L10N: '%s' is the file name of the temporary file
    buf_printf(err, _("Could not create temporary file %s"), buf_string(tempfile));
    goto done;
  }

  print_version(fp_out, false);
  mutt_file_fclose(&fp_out);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(tempfile);

  pview.banner = cmd->name;
  pview.flags = MUTT_PAGER_NO_FLAGS;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&tempfile);
  return rc;
}

/**
 * source_stack_cleanup - Free memory from the stack used for the source command
 */
void source_stack_cleanup(void)
{
  mutt_list_free(&MuttrcStack);
}

/**
 * MuttCommands - General NeoMutt Commands
 */
static const struct Command MuttCommands[] = {
  // clang-format off
  { "alias", CMD_ALIAS, parse_alias, CMD_NO_DATA,
        N_("Define an alias (name to email address)"),
        N_("alias [ -group <name> ... ] <key> <address> [, <address> ... ]"),
        "configuration.html#alias" },
  { "alternates", CMD_ALTERNATES, parse_alternates, CMD_NO_DATA,
        N_("Define a list of alternate email addresses for the user"),
        N_("alternates [ -group <name> ... ] <regex> [ <regex> ... ]"),
        "configuration.html#alternates" },
  { "alternative_order", CMD_ALTERNATIVE_ORDER, parse_stailq, IP &AlternativeOrderList,
        N_("Set preference order for multipart alternatives"),
        N_("alternative_order <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]"),
        "mimesupport.html#alternative-order" },
  { "attachments", CMD_ATTACHMENTS, parse_attachments, CMD_NO_DATA,
        N_("Set attachment counting rules"),
        N_("attachments { + | - }<disposition> <mime-type> [ <mime-type> ... ] | ?"),
        "mimesupport.html#attachments" },
  { "auto_view", CMD_AUTO_VIEW, parse_stailq, IP &AutoViewList,
        N_("Automatically display specified MIME types inline"),
        N_("auto_view <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]"),
        "mimesupport.html#auto-view" },
  { "cd", CMD_CD, parse_cd, CMD_NO_DATA,
        N_("Change NeoMutt's current working directory"),
        N_("cd [ <directory> ]"),
        "configuration.html#cd" },
  { "color", CMD_COLOR, parse_color, CMD_NO_DATA,
        N_("Define colors for the user interface"),
        N_("color <object> [ <attribute> ... ] <foreground> <background> [ <regex> [ <num> ]]"),
        "configuration.html#color" },
  { "echo", CMD_ECHO, parse_echo, CMD_NO_DATA,
        N_("Print a message to the status line"),
        N_("echo <message>"),
        "advancedusage.html#echo" },
  { "finish", CMD_FINISH, parse_finish, CMD_NO_DATA,
        N_("Stop reading current config file"),
        N_("finish "),
        "optionalfeatures.html#ifdef" },
  { "group", CMD_GROUP, parse_group, CMD_NO_DATA,
        N_("Add addresses to an address group"),
        N_("group [ -group <name> ... ] { -rx <regex> ... | -addr <address> ... }"),
        "configuration.html#addrgroup" },
  { "hdr_order", CMD_HDR_ORDER, parse_stailq, IP &HeaderOrderList,
        N_("Define custom order of headers displayed"),
        N_("hdr_order <header> [ <header> ... ]"),
        "configuration.html#hdr-order" },
  { "ifdef", CMD_IFDEF, parse_ifdef, CMD_NO_DATA,
        N_("Conditionally include config commands if symbol defined"),
        N_("ifdef <symbol> '<config-command> [ <args> ... ]'"),
        "optionalfeatures.html#ifdef" },
  { "ifndef", CMD_IFNDEF, parse_ifdef, CMD_NO_DATA,
        N_("Conditionally include if symbol is not defined"),
        N_("ifndef <symbol> '<config-command> [ <args> ... ]'"),
        "optionalfeatures.html#ifdef" },
  { "ignore", CMD_IGNORE, parse_ignore, CMD_NO_DATA,
        N_("Hide specified headers when displaying messages"),
        N_("ignore { * | <string> ... }"),
        "configuration.html#ignore" },
  { "lists", CMD_LISTS, parse_lists, CMD_NO_DATA,
        N_("Add address to the list of mailing lists"),
        N_("lists [ -group <name> ... ] <regex> [ <regex> ... ]"),
        "configuration.html#lists" },
  { "mailboxes", CMD_MAILBOXES, parse_mailboxes, CMD_NO_DATA,
        N_("Define a list of mailboxes to watch"),
        N_("mailboxes [[ -label <label> ] | -nolabel ] [[ -notify | -nonotify ] [ -poll | -nopoll ] <mailbox> ] [ ... ]"),
        "configuration.html#mailboxes" },
  { "mailto_allow", CMD_MAILTO_ALLOW, parse_stailq, IP &MailToAllow,
        N_("Permit specific header-fields in mailto URL processing"),
        N_("mailto_allow { * | <header-field> ... }"),
        "configuration.html#mailto-allow" },
  { "mime_lookup", CMD_MIME_LOOKUP, parse_stailq, IP &MimeLookupList,
        N_("Map specified MIME types/subtypes to display handlers"),
        N_("mime_lookup <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]"),
        "mimesupport.html#mime-lookup" },
  { "mono", CMD_MONO, parse_mono, CMD_NO_DATA,
        N_("**Deprecated**: Use `color`"),
        N_("mono <object> <attribute> [ <pattern> | <regex> ]"),
        "configuration.html#color-mono" },
  { "my_hdr", CMD_MY_HDR, parse_my_hdr, CMD_NO_DATA,
        N_("Add a custom header to outgoing messages"),
        N_("my_hdr <string>"),
        "configuration.html#my-hdr" },
  { "named-mailboxes", CMD_NAMED_MAILBOXES, parse_mailboxes, MUTT_NAMED,
        N_("Define a list of labelled mailboxes to watch"),
        N_("named-mailboxes <description> <mailbox> [ <description> <mailbox> ... ]"),
        "configuration.html#mailboxes" },
  { "nospam", CMD_NOSPAM, parse_nospam, CMD_NO_DATA,
        N_("Remove a spam detection rule"),
        N_("nospam { * | <regex> }"),
        "configuration.html#spam" },
  { "reset", CMD_RESET, parse_set, CMD_NO_DATA,
        N_("Reset a config option to its initial value"),
        N_("reset <variable> [ <variable> ... ]"),
        "configuration.html#set" },
  { "score", CMD_SCORE, parse_score, CMD_NO_DATA,
        N_("Set a score value on emails matching a pattern"),
        N_("score <pattern> <value>"),
        "configuration.html#score-command" },
  { "set", CMD_SET, parse_set, CMD_NO_DATA,
        N_("Set a config variable"),
        N_("set { [ no | inv | & ] <variable> [?] | <variable> [=|+=|-=] value } [ ... ]"),
        "configuration.html#set" },
  { "setenv", CMD_SETENV, parse_setenv, CMD_NO_DATA,
        N_("Set an environment variable"),
        N_("setenv { <variable>? | <variable> <value> }"),
        "advancedusage.html#setenv" },
  { "source", CMD_SOURCE, parse_source, CMD_NO_DATA,
        N_("Read and execute commands from a config file"),
        N_("source <filename>"),
        "configuration.html#source" },
  { "spam", CMD_SPAM, parse_spam, CMD_NO_DATA,
        N_("Define rules to parse spam detection headers"),
        N_("spam <regex> <format>"),
        "configuration.html#spam" },
  { "subjectrx", CMD_SUBJECTRX, parse_subjectrx_list, CMD_NO_DATA,
        N_("Apply regex-based rewriting to message subjects"),
        N_("subjectrx <regex> <replacement>"),
        "advancedusage.html#display-munging" },
  { "subscribe", CMD_SUBSCRIBE, parse_subscribe, CMD_NO_DATA,
        N_("Add address to the list of subscribed mailing lists"),
        N_("subscribe [ -group <name> ... ] <regex> [ <regex> ... ]"),
        "configuration.html#lists" },
  { "tag-formats", CMD_TAG_FORMATS, parse_tag_formats, CMD_NO_DATA,
        N_("Define expandos tags"),
        N_("tag-formats <tag> <format-string> { tag format-string ... }"),
        "optionalfeatures.html#custom-tags" },
  { "tag-transforms", CMD_TAG_TRANSFORMS, parse_tag_transforms, CMD_NO_DATA,
        N_("Rules to transform tags into icons"),
        N_("tag-transforms <tag> <transformed-string> { tag transformed-string ... }"),
        "optionalfeatures.html#custom-tags" },
  { "toggle", CMD_TOGGLE, parse_set, CMD_NO_DATA,
        N_("Toggle the value of a boolean/quad config option"),
        N_("toggle <variable> [ <variable> ... ]"),
        "configuration.html#set" },
  { "unalias", CMD_UNALIAS, parse_unalias, CMD_NO_DATA,
        N_("Remove an alias definition"),
        N_("unalias [ -group <name> ... ] { * | <key> ... }"),
        "configuration.html#alias" },
  { "unalternates", CMD_UNALTERNATES, parse_unalternates, CMD_NO_DATA,
        N_("Remove addresses from `alternates` list"),
        N_("unalternates [ -group <name> ... ] { * | <regex> ... }"),
        "configuration.html#alternates" },
  { "unalternative_order", CMD_UNALTERNATIVE_ORDER, parse_unstailq, IP &AlternativeOrderList,
        N_("Remove MIME types from preference order"),
        N_("unalternative_order { * | [ <mime-type>[/<mime-subtype> ] ... ] }"),
        "mimesupport.html#alternative-order" },
  { "unattachments", CMD_UNATTACHMENTS, parse_unattachments, CMD_NO_DATA,
        N_("Remove attachment counting rules"),
        N_("unattachments { * | { + | - }<disposition> <mime-type> [ <mime-type> ... ] }"),
        "mimesupport.html#attachments" },
  { "unauto_view", CMD_UNAUTO_VIEW, parse_unstailq, IP &AutoViewList,
        N_("Remove MIME types from `auto_view` list"),
        N_("unauto_view { * | [ <mime-type>[/<mime-subtype> ] ... ] }"),
        "mimesupport.html#auto-view" },
  { "uncolor", CMD_UNCOLOR, parse_uncolor, CMD_NO_DATA,
        N_("Remove a `color` definition"),
        N_("uncolor <object> { * | <pattern> ... }"),
        "configuration.html#color" },
  { "ungroup", CMD_UNGROUP, parse_group, CMD_NO_DATA,
        N_("Remove addresses from an address `group`"),
        N_("ungroup [ -group <name> ... ] { * | -rx <regex> ... | -addr <address> ... }"),
        "configuration.html#addrgroup" },
  { "unhdr_order", CMD_UNHDR_ORDER, parse_unstailq, IP &HeaderOrderList,
        N_("Remove header from `hdr_order` list"),
        N_("unhdr_order { * | <header> ... }"),
        "configuration.html#hdr-order" },
  { "unignore", CMD_UNIGNORE, parse_unignore, CMD_NO_DATA,
        N_("Remove a header from the `hdr_order` list"),
        N_("unignore { * | <string> ... }"),
        "configuration.html#ignore" },
  { "unlists", CMD_UNLISTS, parse_unlists, CMD_NO_DATA,
        N_("Remove address from the list of mailing lists"),
        N_("unlists [ -group <name> ... ] { * | <regex> ... }"),
        "configuration.html#lists" },
  { "unmailboxes", CMD_UNMAILBOXES, parse_unmailboxes, CMD_NO_DATA,
        N_("Remove mailboxes from the watch list"),
        N_("unmailboxes { * | <mailbox> ... }"),
        "configuration.html#mailboxes" },
  { "unmailto_allow", CMD_UNMAILTO_ALLOW, parse_unstailq, IP &MailToAllow,
        N_("Disallow header-fields in mailto processing"),
        N_("unmailto_allow { * | <header-field> ... }"),
        "configuration.html#mailto-allow" },
  { "unmime_lookup", CMD_UNMIME_LOOKUP, parse_unstailq, IP &MimeLookupList,
        N_("Remove custom MIME-type handlers"),
        N_("unmime_lookup { * | [ <mime-type>[/<mime-subtype> ] ... ] }"),
        "mimesupport.html#mime-lookup" },
  { "unmono", CMD_UNMONO, parse_unmono, CMD_NO_DATA,
        N_("**Deprecated**: Use `uncolor`"),
        N_("unmono <object> { * | <pattern> ... }"),
        "configuration.html#color-mono" },
  { "unmy_hdr", CMD_UNMY_HDR, parse_unmy_hdr, CMD_NO_DATA,
        N_("Remove a header previously added with `my_hdr`"),
        N_("unmy_hdr { * | <field> ... }"),
        "configuration.html#my-hdr" },
  { "unscore", CMD_UNSCORE, parse_unscore, CMD_NO_DATA,
        N_("Remove scoring rules for matching patterns"),
        N_("unscore { * | <pattern> ... }"),
        "configuration.html#score-command" },
  { "unset", CMD_UNSET, parse_set, CMD_NO_DATA,
        N_("Reset a config option to false/empty"),
        N_("unset <variable> [ <variable> ... ]"),
        "configuration.html#set" },
  { "unsetenv", CMD_UNSETENV, parse_setenv, CMD_NO_DATA,
        N_("Unset an environment variable"),
        N_("unsetenv <variable>"),
        "advancedusage.html#setenv" },
  { "unsubjectrx", CMD_UNSUBJECTRX, parse_unsubjectrx_list, CMD_NO_DATA,
        N_("Remove subject-rewriting rules"),
        N_("unsubjectrx { * | <regex> }"),
        "advancedusage.html#display-munging" },
  { "unsubscribe", CMD_UNSUBSCRIBE, parse_unsubscribe, CMD_NO_DATA,
        N_("Remove address from the list of subscribed mailing lists"),
        N_("unsubscribe [ -group <name> ... ] { * | <regex> ... }"),
        "configuration.html#lists" },
  { "version", CMD_VERSION, parse_version, CMD_NO_DATA,
        N_("Show NeoMutt version and build information"),
        N_("version "),
        "configuration.html#version" },

  { NULL, CMD_NONE, NULL, CMD_NO_DATA, NULL, NULL, NULL, CF_NO_FLAGS },
  // clang-format on
};

/**
 * commands_init - Initialize commands array and register default commands
 */
bool commands_init(void)
{
  return commands_register(&NeoMutt->commands, MuttCommands);
}
