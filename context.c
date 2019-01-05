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
#include "mutt/mutt.h"
#include "email/lib.h"
#include "context.h"
#include "globals.h"
#include "mutt_header.h"
#include "mutt_thread.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "pattern.h"
#include "score.h"
#include "sort.h"

/**
 * ctx_free - Free a Context
 * @param ctx Context to free
 */
void ctx_free(struct Context **ctx)
{
  if (!ctx || !*ctx)
    return;

  mailbox_free(&(*ctx)->mailbox);
  FREE(ctx);
}

/**
 * ctx_cleanup - Release memory and initialize a Context object
 * @param ctx Context to cleanup
 */
void ctx_cleanup(struct Context *ctx)
{
  FREE(&ctx->pattern);
  mutt_pattern_free(&ctx->limit_pattern);
  memset(ctx, 0, sizeof(struct Context));
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

    if (WithCrypto)
    {
      /* NOTE: this _must_ be done before the check for mailcap! */
      e->security = crypt_query(e->content);
    }

    if (!ctx->pattern)
    {
      m->v2r[m->vcount] = msgno;
      e->virtual = m->vcount++;
    }
    else
      e->virtual = -1;
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
        if (Score)
          mutt_score_message(ctx->mailbox, e2, true);
      }
    }

    /* add this message to the hash tables */
    if (m->id_hash && e->env->message_id)
      mutt_hash_insert(m->id_hash, e->env->message_id, e);
    if (m->subj_hash && e->env->real_subj)
      mutt_hash_insert(m->subj_hash, e->env->real_subj, e);
    mutt_label_hash_add(m, e);

    if (Score)
      mutt_score_message(ctx->mailbox, e, false);

    if (e->changed)
      m->changed = true;
    if (e->flagged)
      m->msg_flagged++;
    if (e->deleted)
      m->msg_deleted++;
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
    if (!m->emails[i]->quasi_deleted &&
        ((committing && (!m->emails[i]->deleted || (m->magic == MUTT_MAILDIR && MaildirTrash))) ||
         (!committing && m->emails[i]->active)))
    {
      if (i != j)
      {
        m->emails[j] = m->emails[i];
        m->emails[i] = NULL;
      }
      m->emails[j]->msgno = j;
      if (m->emails[j]->virtual != -1)
      {
        m->v2r[m->vcount] = j;
        m->emails[j]->virtual = m->vcount++;
        struct Body *b = m->emails[j]->content;
        ctx->vsize += b->length + b->offset - b->hdr_offset + padding;
      }

      if (committing)
        m->emails[j]->changed = false;
      else if (m->emails[j]->changed)
        m->changed = true;

      if (!committing || (m->magic == MUTT_MAILDIR && MaildirTrash))
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
      if (m->magic == MUTT_MH || m->magic == MUTT_MAILDIR)
      {
        m->size -= (m->emails[i]->content->length + m->emails[i]->content->offset -
                    m->emails[i]->content->hdr_offset);
      }
      /* remove message from the hash tables */
      if (m->subj_hash && m->emails[i]->env->real_subj)
        mutt_hash_delete(m->subj_hash, m->emails[i]->env->real_subj, m->emails[i]);
      if (m->id_hash && m->emails[i]->env->message_id)
        mutt_hash_delete(m->id_hash, m->emails[i]->env->message_id, m->emails[i]);
      mutt_label_hash_remove(m, m->emails[i]);
      /* The path mx_mbox_check() -> imap_check_mailbox() ->
       *          imap_expunge_mailbox() -> ctx_update_tables()
       * can occur before a call to mx_mbox_sync(), resulting in
       * last_tag being stale if it's not reset here.
       */
      if (ctx->last_tag == m->emails[i])
        ctx->last_tag = NULL;
      mutt_email_free(&m->emails[i]);
    }
  }
  m->msg_count = j;
}

/**
 * ctx_mailbox_changed - Act on a Mailbox change notification
 * @param m      Mailbox
 * @param action Event occurring
 * @param ndata  Private notification data
 */
void ctx_mailbox_changed(struct Mailbox *m, enum MailboxNotification action)
{
  if (!m || !m->ndata)
    return;

  struct Context *ctx = m->ndata;

  switch (action)
  {
    case MBN_CLOSED:
      mutt_clear_threads(ctx);
      ctx_cleanup(ctx);
      break;
    case MBN_INVALID:
      ctx_update(ctx);
      break;
    case MBN_RESORT:
      ctx_update_tables(ctx, false);
      mutt_sort_headers(ctx, true);
      break;
  }
}

