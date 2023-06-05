/**
 * @file
 * Subject Regex handling
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
#include <stdint.h>
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
 * parse_unreplace_list - Remove a string replacement rule - Implements Command::parse() - @ingroup command_parse
 */
static enum CommandResult parse_unreplace_list(struct Buffer *buf, struct Buffer *s,
                                               struct ReplaceList *list, struct Buffer *err)
{
  /* First token is a regex. */
  if (!MoreArgs(s))
  {
    buf_printf(err, _("%s: too few arguments"), "unsubjectrx");
    return MUTT_CMD_WARNING;
  }

  parse_extract_token(buf, s, TOKEN_NO_FLAGS);

  /* "*" is a special case. */
  if (mutt_str_equal(buf->data, "*"))
  {
    mutt_replacelist_free(list);
    return MUTT_CMD_SUCCESS;
  }

  mutt_replacelist_remove(list, buf->data);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_replace_list - Parse a string replacement rule - Implements Command::parse() - @ingroup command_parse
 */
static enum CommandResult parse_replace_list(struct Buffer *buf, struct Buffer *s,
                                             struct ReplaceList *list, struct Buffer *err)
{
  struct Buffer templ = buf_make(0);

  /* First token is a regex. */
  if (!MoreArgs(s))
  {
    buf_printf(err, _("%s: too few arguments"), "subjectrx");
    return MUTT_CMD_WARNING;
  }
  parse_extract_token(buf, s, TOKEN_NO_FLAGS);

  /* Second token is a replacement template */
  if (!MoreArgs(s))
  {
    buf_printf(err, _("%s: too few arguments"), "subjectrx");
    return MUTT_CMD_WARNING;
  }
  parse_extract_token(&templ, s, TOKEN_NO_FLAGS);

  if (mutt_replacelist_add(list, buf->data, templ.data, err) != 0)
  {
    FREE(&templ.data);
    return MUTT_CMD_ERROR;
  }
  FREE(&templ.data);

  return MUTT_CMD_SUCCESS;
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

  env->disp_subj = mutt_replacelist_apply(&SubjectRegexList, NULL, 0, env->subject);
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
 */
enum CommandResult parse_subjectrx_list(struct Buffer *buf, struct Buffer *s,
                                        intptr_t data, struct Buffer *err)
{
  enum CommandResult rc;

  rc = parse_replace_list(buf, s, &SubjectRegexList, err);
  if (rc == MUTT_CMD_SUCCESS)
  {
    mutt_debug(LL_NOTIFY, "NT_SUBJRX_ADD: %s\n", buf->data);
    notify_send(SubjRxNotify, NT_SUBJRX, NT_SUBJRX_ADD, NULL);
  }
  return rc;
}

/**
 * parse_unsubjectrx_list - Parse the 'unsubjectrx' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult parse_unsubjectrx_list(struct Buffer *buf, struct Buffer *s,
                                          intptr_t data, struct Buffer *err)
{
  enum CommandResult rc;

  rc = parse_unreplace_list(buf, s, &SubjectRegexList, err);
  if (rc == MUTT_CMD_SUCCESS)
  {
    mutt_debug(LL_NOTIFY, "NT_SUBJRX_DELETE: %s\n", buf->data);
    notify_send(SubjRxNotify, NT_SUBJRX, NT_SUBJRX_DELETE, NULL);
  }
  return rc;
}
