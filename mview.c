/**
 * @file
 * View of a Mailbox
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020-2021 Pietro Cerutti <gahr@gahr.ch>
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
 * @page neo_mview View of a Mailbox
 *
 * View of a Mailbox
 */

#include "config.h"
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mview.h"
#include "ncrypt/lib.h"
#include "pattern/lib.h"
#include "mutt_header.h"
#include "mutt_thread.h"
#include "score.h"

/**
 * mview_update - Update the MailboxView's message counts
 * @param mv Mailbox View
 *
 * this routine is called to update the counts in the MailboxView structure
 */
void mview_update(struct MailboxView *mv)
{
  if (!mv || !mv->mailbox)
    return;

  struct Mailbox *m = mv->mailbox;

  mutt_hash_free(&m->subj_hash);
  mutt_hash_free(&m->id_hash);

  /* reset counters */
  m->msg_unread = 0;
  m->msg_flagged = 0;
  m->msg_new = 0;
  m->msg_deleted = 0;
  m->msg_tagged = 0;
  mv->vcount = 0;
  m->changed = false;

  mutt_clear_threads(mv->threads);

  const bool c_score = cs_subset_bool(NeoMutt->sub, "score");
  struct Email *e = NULL;
  for (int msgno = 0; msgno < m->msg_count; msgno++)
  {
    e = m->emails[msgno];
    if (!e)
      continue;

    if (WithCrypto)
    {
      /* NOTE: this _must_ be done before the check for mailcap! */
      e->security = crypt_query(e->body);
    }

    if (mview_has_limit(mv))
    {
      e->vnum = -1;
    }
    else
    {
      eview_free(&mv->v2r[mv->vcount]);
      mv->v2r[mv->vcount] = eview_new(e);
      e->vnum = mv->vcount++;
    }
    e->msgno = msgno;

    if (e->env->supersedes)
    {
      struct Email *e2 = NULL;

      if (!m->id_hash)
        m->id_hash = mutt_make_id_hash(m);

      e2 = mutt_hash_find(m->id_hash, e->env->supersedes);
      if (e2)
      {
        e2->superseded = true;
        if (c_score)
          mutt_score_message(mv->mailbox, e2, true);
      }
    }

    /* add this message to the hash tables */
    if (m->id_hash && e->env->message_id)
      mutt_hash_insert(m->id_hash, e->env->message_id, e);
    if (m->subj_hash && e->env->real_subj)
      mutt_hash_insert(m->subj_hash, e->env->real_subj, e);
    mutt_label_hash_add(m, e);

    if (c_score)
      mutt_score_message(mv->mailbox, e, false);

    if (e->changed)
      m->changed = true;
    if (e->flagged)
      m->msg_flagged++;
    if (e->deleted)
      m->msg_deleted++;
    if (e->tagged)
      m->msg_tagged++;
    if (!e->read)
    {
      m->msg_unread++;
      if (!e->old)
        m->msg_new++;
    }
  }

  /* rethread from scratch */
  mutt_sort_headers(mv, true);
}

/**
 * mview_free - Free a MailboxView
 * @param[out] ptr MailboxView to free
 */
void mview_free(struct MailboxView **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct MailboxView *mv = *ptr;

  struct EventMview ev_m = { mv };
  mutt_debug(LL_NOTIFY, "NT_MVIEW_DELETE: %p\n", (void *) mv);
  notify_send(mv->notify, NT_MVIEW, NT_MVIEW_DELETE, &ev_m);

  if (mv->mailbox)
  {
    notify_observer_remove(mv->mailbox->notify, mview_mailbox_observer, mv);

    // Disconnect the Emails before freeing the Threads
    for (int i = 0; i < mv->mailbox->msg_count; i++)
    {
      struct Email *e = mv->mailbox->emails[i];
      if (!e)
        continue;
      e->thread = NULL;
      e->threaded = false;
    }
  }

  mutt_thread_ctx_free(&mv->threads);
  notify_free(&mv->notify);
  FREE(&mv->pattern);
  mutt_pattern_free(&mv->limit_pattern);

  for (size_t i = 0; i < mv->vcount; i++)
    eview_free(&mv->v2r[i]);

  FREE(&mv->v2r);
  *ptr = NULL;
  FREE(&mv);
}

/**
 * mview_new - Create a new MailboxView
 * @param m      Mailbox
 * @param parent Notification parent
 * @retval ptr New MailboxView
 */
struct MailboxView *mview_new(struct Mailbox *m, struct Notify *parent)
{
  if (!m)
    return NULL;

  struct MailboxView *mv = mutt_mem_calloc(1, sizeof(struct MailboxView));

  mv->notify = notify_new();
  notify_set_parent(mv->notify, parent);
  struct EventMview ev_m = { mv };
  mutt_debug(LL_NOTIFY, "NT_MVIEW_ADD: %p\n", (void *) mv);
  notify_send(mv->notify, NT_MVIEW, NT_MVIEW_ADD, &ev_m);
  // If the Mailbox is closed, mv->mailbox must be set to NULL
  notify_observer_add(m->notify, NT_MAILBOX, mview_mailbox_observer, mv);

  mv->mailbox = m;
  mv->v2r = mutt_mem_calloc(m->email_max, sizeof(struct Email *));
  mv->threads = mutt_thread_ctx_init(mv);
  mv->msg_in_pager = -1;
  mv->collapsed = false;
  mview_update(mv);

  return mv;
}

/**
 * mview_cleanup - Release memory and initialize a MailboxView
 * @param mv MailboxView to cleanup
 */
static void mview_cleanup(struct MailboxView *mv)
{
  FREE(&mv->pattern);
  mutt_pattern_free(&mv->limit_pattern);
  if (mv->mailbox)
    notify_observer_remove(mv->mailbox->notify, mview_mailbox_observer, mv);

  struct Notify *notify = mv->notify;
  struct Mailbox *m = mv->mailbox;
  memset(mv, 0, sizeof(struct MailboxView));
  mv->notify = notify;
  mv->mailbox = m;
}

/**
 * mview_mailbox_observer - Notification that a Mailbox has changed - Implements ::observer_t - @ingroup observer_api
 */
int mview_mailbox_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_MAILBOX)
    return 0;
  if (!nc->global_data)
    return -1;

  struct MailboxView *mv = nc->global_data;

  if (nc->event_subtype == NT_MAILBOX_DELETE)
  {
    mutt_clear_threads(mv->threads);
    mview_cleanup(mv);
  }

  mutt_debug(LL_DEBUG5, "mailbox done\n");
  return 0;
}

/**
 * message_is_tagged - Is a message in the index tagged (and within limit)
 * @param e   Email
 * @retval true The message is both tagged and within limit
 *
 * If a limit is in effect, the message must be visible within it.
 */
bool message_is_tagged(struct Email *e)
{
  return e->visible && e->tagged;
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

      ARRAY_ADD(ea, e);
    }
  }
  else
  {
    if (!e)
      return -1;

    ARRAY_ADD(ea, e);
  }

  return ARRAY_SIZE(ea);
}

/**
 * mutt_get_virt_email - Get a virtual Email
 * @param mv   Mailbox view
 * @param vnum Virtual index number
 * @retval ptr  Email
 * @retval NULL No Email selected, or bad index values
 *
 * This safely gets the result of the following:
 * - `mailbox->emails[mailbox->v2r[vnum]]`
 */
struct Email *mutt_get_virt_email(struct MailboxView *mv, int vnum)
{
  if (!mv || !mv->v2r)
    return NULL;

  if ((vnum < 0) || (vnum >= mv->vcount))
    return NULL;

  return mv->v2r[vnum]->email;
}

/**
 * mview_has_limit - Is a limit active?
 * @param mv MailboxView
 * @retval true A limit is active
 * @retval false No limit is active
 */
bool mview_has_limit(const struct MailboxView *mv)
{
  return mv && mv->pattern;
}

/**
 * mview_mailbox - Wrapper to get the mailbox in a MailboxView, or NULL
 * @param mv MailboxView
 * @retval ptr The mailbox in the MailboxView
 * @retval NULL MailboxView is NULL or doesn't have a mailbox
 */
struct Mailbox *mview_mailbox(struct MailboxView *mv)
{
  return mv ? mv->mailbox : NULL;
}

/**
 * top_of_thread - Find the first email in the current thread
 * @param e Current Email
 * @retval ptr  Success, email found
 * @retval NULL Error
 */
static struct MuttThread *top_of_thread(struct Email *e)
{
  if (!e)
    return NULL;

  struct MuttThread *t = e->thread;

  while (t && t->parent)
    t = t->parent;

  return t;
}

/**
 * mutt_limit_current_thread - Limit the email view to the current thread
 * @param mv Mailbox View
 * @param e  Email
 * @retval true Success
 * @retval false Failure
 */
bool mutt_limit_current_thread(struct MailboxView *mv, struct Email *e)
{
  if (!mv || !mv->mailbox || !e)
    return false;

  struct Mailbox *m = mv->mailbox;

  struct MuttThread *me = top_of_thread(e);
  if (!me)
    return false;

  mv->vcount = 0;
  mv->vsize = 0;
  mv->collapsed = false;

  for (int i = 0; i < m->msg_count; i++)
  {
    e = m->emails[i];
    if (!e)
      break;

    e->vnum = -1;
    e->visible = false;
    e->collapsed = false;
    e->num_hidden = 0;

    if (top_of_thread(e) == me)
    {
      struct Body *body = e->body;

      e->vnum = mv->vcount;
      e->visible = true;
      eview_free(&mv->v2r[mv->vcount]);
      mv->v2r[mv->vcount] = eview_new(e);
      mv->vcount++;
      mv->vsize += (body->length + body->offset - body->hdr_offset);
    }
  }
  return true;
}
