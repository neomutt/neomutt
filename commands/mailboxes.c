/**
 * @file
 * Parse Mailboxes Commands
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page commands_mailboxes Parse Mailboxes Commands
 *
 * Parse Mailboxes Commands
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "mailboxes.h"
#include "parse/lib.h"
#include "muttlib.h"
#include "mx.h"
#ifdef USE_INOTIFY
#include "monitor.h"
#endif

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
