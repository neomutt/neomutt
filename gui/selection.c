/**
 * @file
 * Build a selection of Emails for an action
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page gui_selection Build a selection of Emails for an action
 *
 * Build a selection of Emails for an action
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mview.h"

/**
 * ea_contains_email - Does a working set already include an Email?
 * @param ea Selected emails
 * @param e  Candidate email
 * @retval true Email is selected
 */
static bool ea_contains_email(struct EmailArray *ea, struct Email *e)
{
  if (!ea || !e)
    return false;

  struct Email **ep = NULL;
  ARRAY_FOREACH(ep, ea)
  {
    if (*ep == e)
      return true;
  }

  return false;
}

/**
 * ea_add_email - Add an Email to a working set
 * @param ea Selected emails
 * @param e  Email to add
 */
static void ea_add_email(struct EmailArray *ea, struct Email *e)
{
  if (!ea_contains_email(ea, e))
    ARRAY_ADD(ea, e);
}

/**
 * ea_add_collapsed_thread - Add all emails in a collapsed thread
 * @param ea Selected emails
 * @param e  Collapsed email
 */
static void ea_add_collapsed_thread(struct EmailArray *ea, struct Email *e)
{
  if (!e || !e->collapsed || !e->thread)
    return;

  struct MuttThread *top = e->thread;
  while (top->parent)
    top = top->parent;

  struct MuttThread *thread = top;
  while (thread)
  {
    struct Email *et = thread->message;
    if (et && et->visible)
      ea_add_email(ea, et);

    if (thread->child)
    {
      thread = thread->child;
      continue;
    }

    // Leaf node: backtrack to find the next sibling,
    // but stay within the subtree rooted at `top`
    while ((thread != top) && !thread->next)
      thread = thread->parent;

    if (thread == top)
      break;

    thread = thread->next;
  }
}

/**
 * ea_add_folded - Add an Email, expanding collapsed threads
 * @param ea Selected emails
 * @param e  Email to add
 */
static void ea_add_folded(struct EmailArray *ea, struct Email *e)
{
  ea_add_email(ea, e);
  ea_add_collapsed_thread(ea, e);
}

/**
 * ea_add_tagged - Get an array of the tagged Emails
 * @param ea         Empty EmailArray to populate
 * @param mv         Current Mailbox
 * @param e          Current Email
 * @param use_tagged Use tagged Emails
 * @retval num Number of selected emails
 * @retval -1  Error
 */
int ea_add_tagged(struct EmailArray *ea, struct MailboxView *mv, struct Email *e, bool use_tagged)
{
  if (use_tagged)
  {
    if (!mv || !mv->mailbox || !mv->mailbox->emails)
      return -1;

    struct Mailbox *m = mv->mailbox;
    for (int i = 0; i < m->msg_count; i++)
    {
      e = m->emails[i];
      if (!e)
        break;
      if (!message_is_tagged(e))
        continue;

      ea_add_folded(ea, e);
    }
  }
  else
  {
    if (!e)
      return -1;

    ea_add_folded(ea, e);
  }

  return ARRAY_SIZE(ea);
}
