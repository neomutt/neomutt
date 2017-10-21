/**
 * @file
 * Representation of an email header (envelope)
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include <stddef.h>
#include "lib/buffer.h"
#include "lib/memory.h"
#include "envelope.h"
#include "lib/queue.h"
#include "rfc822.h"

struct Envelope *mutt_new_envelope(void)
{
  struct Envelope *e = safe_calloc(1, sizeof(struct Envelope));
  STAILQ_INIT(&e->references);
  STAILQ_INIT(&e->in_reply_to);
  STAILQ_INIT(&e->userhdrs);
  return e;
}

void mutt_free_envelope(struct Envelope **p)
{
  if (!*p)
    return;
  rfc822_free_address(&(*p)->return_path);
  rfc822_free_address(&(*p)->from);
  rfc822_free_address(&(*p)->to);
  rfc822_free_address(&(*p)->cc);
  rfc822_free_address(&(*p)->bcc);
  rfc822_free_address(&(*p)->sender);
  rfc822_free_address(&(*p)->reply_to);
  rfc822_free_address(&(*p)->mail_followup_to);

  FREE(&(*p)->list_post);
  FREE(&(*p)->subject);
  /* real_subj is just an offset to subject and shouldn't be freed */
  FREE(&(*p)->disp_subj);
  FREE(&(*p)->message_id);
  FREE(&(*p)->supersedes);
  FREE(&(*p)->date);
  FREE(&(*p)->x_label);
  FREE(&(*p)->organization);
#ifdef USE_NNTP
  FREE(&(*p)->newsgroups);
  FREE(&(*p)->xref);
  FREE(&(*p)->followup_to);
  FREE(&(*p)->x_comment_to);
#endif

  mutt_buffer_free(&(*p)->spam);

  mutt_list_free(&(*p)->references);
  mutt_list_free(&(*p)->in_reply_to);
  mutt_list_free(&(*p)->userhdrs);
  FREE(p);
}

/**
 * mutt_merge_envelopes - Merge the headers of two emails
 *
 * Move all the headers from extra not present in base into base
 */
void mutt_merge_envelopes(struct Envelope *base, struct Envelope **extra)
{
/* copies each existing element if necessary, and sets the element
  * to NULL in the source so that mutt_free_envelope doesn't leave us
  * with dangling pointers. */
#define MOVE_ELEM(h)                                                           \
  if (!base->h)                                                                \
  {                                                                            \
    base->h = (*extra)->h;                                                     \
    (*extra)->h = NULL;                                                        \
  }

#define MOVE_STAILQ(h)                                                         \
  if (STAILQ_EMPTY(&base->h))                                                  \
  {                                                                            \
    STAILQ_SWAP(&base->h, &((*extra))->h, ListNode);                           \
  }

  MOVE_ELEM(return_path);
  MOVE_ELEM(from);
  MOVE_ELEM(to);
  MOVE_ELEM(cc);
  MOVE_ELEM(bcc);
  MOVE_ELEM(sender);
  MOVE_ELEM(reply_to);
  MOVE_ELEM(mail_followup_to);
  MOVE_ELEM(list_post);
  MOVE_ELEM(message_id);
  MOVE_ELEM(supersedes);
  MOVE_ELEM(date);
  MOVE_ELEM(x_label);
  MOVE_ELEM(x_original_to);
  if (!base->refs_changed)
  {
    MOVE_STAILQ(references);
  }
  if (!base->irt_changed)
  {
    MOVE_STAILQ(in_reply_to);
  }

  /* real_subj is subordinate to subject */
  if (!base->subject)
  {
    base->subject = (*extra)->subject;
    base->real_subj = (*extra)->real_subj;
    base->disp_subj = (*extra)->disp_subj;
    (*extra)->subject = NULL;
    (*extra)->real_subj = NULL;
    (*extra)->disp_subj = NULL;
  }
  /* spam and user headers should never be hashed, and the new envelope may
    * have better values. Use new versions regardless. */
  mutt_buffer_free(&base->spam);
  mutt_list_free(&base->userhdrs);
  MOVE_ELEM(spam);
  MOVE_STAILQ(userhdrs);
#undef MOVE_ELEM

  mutt_free_envelope(extra);
}
