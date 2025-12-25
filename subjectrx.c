/**
 * @file
 * Subject Regex handling
 *
 * @authors
 * Copyright (C) 2021-2023 Richard Russon <rich@flatcap.org>
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
 * @page neo_subjrx Subject Regex handling
 *
 * Subject Regex handling
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "subjectrx.h"
#include "parse/lib.h"
#include "mview.h"

/// List of subjectrx rules for modifying the Subject:
static struct ReplaceList SubjectRegexList = STAILQ_HEAD_INITIALIZER(SubjectRegexList);
static struct Notify *SubjRxNotify = NULL; ///< Notifications: #NotifySubjRx

/**
 * subjrx_cleanup - Free the Subject Regex List
 */
void subjrx_cleanup(void)
{
  notify_free(&SubjRxNotify);
  mutt_replacelist_free(&SubjectRegexList);
}

/**
 * subjrx_init - Create new Subject Regex List
 */
void subjrx_init(void)
{
  if (SubjRxNotify)
    return;

  SubjRxNotify = notify_new();
  notify_set_parent(SubjRxNotify, NeoMutt->notify);
}

/**
 * parse_unreplace_list - Remove a string replacement rule
 * @param cmd  Command being parsed
 * @param line Buffer containing string to be parsed
 * @param list ReplaceList to be updated
 * @param err  Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
static enum CommandResult parse_unreplace_list(const struct Command *cmd,
                                               struct Buffer *line,
                                               struct ReplaceList *list, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  /* First token is a regex. */
  parse_extract_token(token, line, TOKEN_NO_FLAGS);

  /* "*" is a special case. */
  if (mutt_str_equal(buf_string(token), "*"))
  {
    mutt_replacelist_free(list);
    buf_pool_release(&token);
    return MUTT_CMD_SUCCESS;
  }

  mutt_replacelist_remove(list, buf_string(token));
  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_replace_list - Parse a string replacement rule
 * @param cmd  Command being parsed
 * @param line Buffer containing string to be parsed
 * @param list ReplaceList to be updated
 * @param err  Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
static enum CommandResult parse_replace_list(const struct Command *cmd, struct Buffer *line,
                                             struct ReplaceList *list, struct Buffer *err)
{
  struct Buffer *templ = buf_pool_get();
  struct Buffer *regex = buf_pool_get();
  int rc = MUTT_CMD_WARNING;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    goto done;
  }

  /* First token is a regex. */
  parse_extract_token(regex, line, TOKEN_NO_FLAGS);

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    goto done;
  }

  /* Second token is a replacement template */
  parse_extract_token(templ, line, TOKEN_NO_FLAGS);

  if (mutt_replacelist_add(list, buf_string(regex), buf_string(templ), err) != 0)
  {
    rc = MUTT_CMD_ERROR;
    goto done;
  }

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&regex);
  buf_pool_release(&templ);
  return rc;
}

/**
 * subjrx_apply_mods - Apply regex modifications to the subject
 * @param env Envelope of Email
 * @retval true Subject modified
 */
bool subjrx_apply_mods(struct Envelope *env)
{
  if (!env || !env->subject || (*env->subject == '\0'))
    return false;

  if (env->disp_subj)
    return true;

  if (STAILQ_EMPTY(&SubjectRegexList))
    return false;

  env->disp_subj = mutt_replacelist_apply(&SubjectRegexList, env->subject);
  return true;
}

/**
 * subjrx_clear_mods - Clear out all modified email subjects
 * @param mv Mailbox view
 */
void subjrx_clear_mods(struct MailboxView *mv)
{
  if (!mv || !mv->mailbox)
    return;

  struct Mailbox *m = mv->mailbox;

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e || !e->env)
      continue;
    FREE(&e->env->disp_subj);
  }
}

/**
 * parse_subjectrx_list - Parse the 'subjectrx' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `subjectrx <regex> <replacement>`
 */
enum CommandResult parse_subjectrx_list(const struct Command *cmd,
                                        struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  enum CommandResult rc;

  rc = parse_replace_list(cmd, line, &SubjectRegexList, err);
  if (rc == MUTT_CMD_SUCCESS)
  {
    mutt_debug(LL_NOTIFY, "NT_SUBJRX_ADD: %s\n", cmd->name);
    notify_send(SubjRxNotify, NT_SUBJRX, NT_SUBJRX_ADD, NULL);
  }
  return rc;
}

/**
 * parse_unsubjectrx_list - Parse the 'unsubjectrx' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unsubjectrx { * | <regex> }`
 */
enum CommandResult parse_unsubjectrx_list(const struct Command *cmd,
                                          struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  enum CommandResult rc;

  rc = parse_unreplace_list(cmd, line, &SubjectRegexList, err);
  if (rc == MUTT_CMD_SUCCESS)
  {
    mutt_debug(LL_NOTIFY, "NT_SUBJRX_DELETE: %s\n", cmd->name);
    notify_send(SubjRxNotify, NT_SUBJRX, NT_SUBJRX_DELETE, NULL);
  }
  return rc;
}
