/**
 * @file
 * The "currently-open" mailbox
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page ctx The "currently-open" mailbox
 *
 * The "currently-open" mailbox
 */

#include "config.h"
#include <string.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "context.h"
#include "mutt_globals.h"
#include "mutt_header.h"
#include "mutt_thread.h"
#include "mx.h"
#include "score.h"
#include "sort.h"
#include "maildir/lib.h"
#include "ncrypt/lib.h"
#include "pattern/lib.h"

/**
 * ctx_free - Free a Context
 * @param[out] ptr Context to free
 */
void ctx_free(struct Context **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Context *ctx = *ptr;

  struct EventContext ev_ctx = { ctx };
  notify_send(ctx->notify, NT_CONTEXT, NT_CONTEXT_CLOSE, &ev_ctx);

  if (ctx->mailbox)
    notify_observer_remove(ctx->mailbox->notify, ctx_mailbox_observer, ctx);

  mutt_hash_free(&ctx->thread_hash);
  notify_free(&ctx->notify);

  FREE(ptr);
}

/**
 * ctx_new - Create a new Context
 * @retval ptr New Context
 */
struct Context *ctx_new(void)
{
  struct Context *ctx = mutt_mem_calloc(1, sizeof(struct Context));

  ctx->notify = notify_new();
  notify_set_parent(ctx->notify, NeoMutt->notify);

  return ctx;
}

/**
 * ctx_cleanup - Release memory and initialize a Context object
 * @param ctx Context to cleanup
 */
static void ctx_cleanup(struct Context *ctx)
{
  FREE(&ctx->pattern);
  mutt_pattern_free(&ctx->limit_pattern);
  if (ctx->mailbox)
    notify_observer_remove(ctx->mailbox->notify, ctx_mailbox_observer, ctx);

  struct Notify *notify = ctx->notify;
  struct Mailbox *m = ctx->mailbox;
  memset(ctx, 0, sizeof(struct Context));
  ctx->notify = notify;
  ctx->mailbox = m;
}

/**
 * ctx_update - Update the Context's message counts
 * @param ctx          Mailbox
 *
 * this routine is called to update the counts in the context structure
 */
void ctx_update(struct Context *ctx)
{
  if (!ctx || !ctx->mailbox)
    return;

  struct Mailbox *m = ctx->mailbox;

  mutt_hash_free(&m->subj_hash);
  mutt_hash_free(&m->id_hash);

  /* reset counters */
  m->msg_unread = 0;
  m->msg_flagged = 0;
  m->msg_new = 0;
  m->msg_deleted = 0;
  m->msg_tagged = 0;
  m->vcount = 0;
  m->changed = false;

  mutt_clear_threads(ctx);

  struct Email *e = NULL;
  for (int msgno = 0; msgno < m->msg_count; msgno++)
  {
    e = m->emails[msgno];
    if (!e)
      continue;

    if (WithCrypto)
    {
      /* NOTE: this _must_ be done before the check for mailcap! */
      e->security = crypt_query(e->content);
    }

    if (ctx->pattern)
    {
      e->vnum = -1;
    }
    else
    {
      m->v2r[m->vcount] = msgno;
      e->vnum = m->vcount++;
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
        if (C_Score)
          mutt_score_message(ctx->mailbox, e2, true);
      }
    }

    /* add this message to the hash tables */
    if (m->id_hash && e->env->message_id)
      mutt_hash_insert(m->id_hash, e->env->message_id, e);
    if (m->subj_hash && e->env->real_subj)
      mutt_hash_insert(m->subj_hash, e->env->real_subj, e);
    mutt_label_hash_add(m, e);

    if (C_Score)
      mutt_score_message(ctx->mailbox, e, false);

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

  mutt_sort_headers(ctx, true); /* rethread from scratch */
}

/**
 * ctx_update_tables - Update a Context structure's internal tables
 * @param ctx        Mailbox
 * @param committing Commit the changes?
 */
void ctx_update_tables(struct Context *ctx, bool committing)
{
  if (!ctx || !ctx->mailbox)
    return;

  struct Mailbox *m = ctx->mailbox;

  int i, j, padding;

  /* update memory to reflect the new state of the mailbox */
  m->vcount = 0;
  ctx->vsize = 0;
  m->msg_tagged = 0;
  m->msg_deleted = 0;
  m->msg_new = 0;
  m->msg_unread = 0;
  m->changed = false;
  m->msg_flagged = 0;
  padding = mx_msg_padding_size(m);
  for (i = 0, j = 0; i < m->msg_count; i++)
  {
    if (!m->emails[i])
      break;
    if (!m->emails[i]->quasi_deleted &&
        ((committing && (!m->emails[i]->deleted || ((m->type == MUTT_MAILDIR) && C_MaildirTrash))) ||
         (!committing && m->emails[i]->active)))
    {
      if (i != j)
      {
        m->emails[j] = m->emails[i];
        m->emails[i] = NULL;
      }
      m->emails[j]->msgno = j;
      if (m->emails[j]->vnum != -1)
      {
        m->v2r[m->vcount] = j;
        m->emails[j]->vnum = m->vcount++;
        struct Body *b = m->emails[j]->content;
        ctx->vsize += b->length + b->offset - b->hdr_offset + padding;
      }

      if (committing)
      {
        m->emails[j]->changed = false;
        m->emails[j]->env->changed = false;
      }
      else if (m->emails[j]->changed)
        m->changed = true;

      if (!committing || ((m->type == MUTT_MAILDIR) && C_MaildirTrash))
      {
        if (m->emails[j]->deleted)
          m->msg_deleted++;
      }

      if (m->emails[j]->tagged)
        m->msg_tagged++;
      if (m->emails[j]->flagged)
        m->msg_flagged++;
      if (!m->emails[j]->read)
      {
        m->msg_unread++;
        if (!m->emails[j]->old)
          m->msg_new++;
      }

      j++;
    }
    else
    {
      if ((m->type == MUTT_NOTMUCH) || (m->type == MUTT_MH) ||
          (m->type == MUTT_MAILDIR) || (m->type == MUTT_IMAP))
      {
        mailbox_size_sub(m, m->emails[i]);
      }
      /* remove message from the hash tables */
      if (m->subj_hash && m->emails[i]->env->real_subj)
        mutt_hash_delete(m->subj_hash, m->emails[i]->env->real_subj, m->emails[i]);
      if (m->id_hash && m->emails[i]->env->message_id)
        mutt_hash_delete(m->id_hash, m->emails[i]->env->message_id, m->emails[i]);
      mutt_label_hash_remove(m, m->emails[i]);
      email_free(&m->emails[i]);
    }
  }
  m->msg_count = j;
}

/**
 * ctx_mailbox_observer - Watch for changes affecting the Context - Implements ::observer_t
 */
int ctx_mailbox_observer(struct NotifyCallback *nc)
{
  if (!nc->global_data)
    return -1;
  if (nc->event_type != NT_MAILBOX)
    return 0;

  struct Context *ctx = nc->global_data;

  switch (nc->event_subtype)
  {
    case NT_MAILBOX_CLOSED:
      mutt_clear_threads(ctx);
      ctx_cleanup(ctx);
      break;
    case NT_MAILBOX_INVALID:
      ctx_update(ctx);
      break;
    case NT_MAILBOX_UPDATE:
      ctx_update_tables(ctx, true);
      break;
    case NT_MAILBOX_RESORT:
      mutt_sort_headers(ctx, true);
      break;
  }

  return 0;
}

/**
 * message_is_visible - Is a message in the index within limit
 * @param ctx Context
 * @param e   Email
 * @retval true The message is within limit
 *
 * If no limit is in effect, all the messages are visible.
 */
bool message_is_visible(struct Context *ctx, struct Email *e)
{
  if (!ctx || !e)
    return false;

  return !ctx->pattern || e->limited;
}

/**
 * message_is_tagged - Is a message in the index tagged (and within limit)
 * @param ctx Open mailbox
 * @param e   Email
 * @retval true The message is both tagged and within limit
 *
 * If a limit is in effect, the message must be visible within it.
 */
bool message_is_tagged(struct Context *ctx, struct Email *e)
{
  return message_is_visible(ctx, e) && e->tagged;
}

/**
 * el_add_tagged - Get a list of the tagged Emails
 * @param el         Empty EmailList to populate
 * @param ctx        Current Mailbox
 * @param e          Current Email
 * @param use_tagged Use tagged Emails
 * @retval num Number of selected emails
 * @retval -1  Error
 */
int el_add_tagged(struct EmailList *el, struct Context *ctx, struct Email *e, bool use_tagged)
{
  int count = 0;

  if (use_tagged)
  {
    if (!ctx || !ctx->mailbox || !ctx->mailbox->emails)
      return -1;

    struct Mailbox *m = ctx->mailbox;
    for (size_t i = 0; i < m->msg_count; i++)
    {
      e = m->emails[i];
      if (!e)
        break;
      if (!message_is_tagged(ctx, e))
        continue;

      struct EmailNode *en = mutt_mem_calloc(1, sizeof(*en));
      en->email = e;
      STAILQ_INSERT_TAIL(el, en, entries);
      count++;
    }
  }
  else
  {
    if (!e)
      return -1;

    struct EmailNode *en = mutt_mem_calloc(1, sizeof(*en));
    en->email = e;
    STAILQ_INSERT_TAIL(el, en, entries);
    count = 1;
  }

  return count;
}

/**
 * mutt_get_virt_email - Get a virtual Email
 * @param m    Mailbox
 * @param vnum Virtual index number
 * @retval ptr  Email
 * @retval NULL No Email selected, or bad index values
 *
 * This safely gets the result of the following:
 * - `mailbox->emails[mailbox->v2r[vnum]]`
 */
struct Email *mutt_get_virt_email(struct Mailbox *m, int vnum)
{
  if (!m || !m->emails || !m->v2r)
    return NULL;

  if ((vnum < 0) || (vnum >= m->vcount))
    return NULL;

  int inum = m->v2r[vnum];
  if ((inum < 0) || (inum >= m->msg_count))
    return NULL;

  return m->emails[inum];
}
