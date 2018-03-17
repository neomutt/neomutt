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

/**
 * @page envelope Representation of an email header (envelope)
 *
 * Representation of an email header (envelope)
 */

#include "config.h"
#include <stddef.h>
#include "mutt/mutt.h"
#include "envelope.h"
#include "protos.h"

/**
 * mutt_env_new - Create a new Envelope
 * @retval ptr New Envelope
 */
struct Envelope *mutt_env_new(void)
{
  struct Envelope *e = mutt_mem_calloc(1, sizeof(struct Envelope));
  STAILQ_INIT(&e->references);
  STAILQ_INIT(&e->in_reply_to);
  STAILQ_INIT(&e->userhdrs);
  return e;
}

/**
 * mutt_env_free - Free an Envelope
 * @param p Envelope to free
 */
void mutt_env_free(struct Envelope **p)
{
  if (!*p)
    return;
  mutt_addr_free(&(*p)->return_path);
  mutt_addr_free(&(*p)->from);
  mutt_addr_free(&(*p)->to);
  mutt_addr_free(&(*p)->cc);
  mutt_addr_free(&(*p)->bcc);
  mutt_addr_free(&(*p)->sender);
  mutt_addr_free(&(*p)->reply_to);
  mutt_addr_free(&(*p)->mail_followup_to);

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
 * mutt_env_merge - Merge the headers of two Envelopes
 * @param base  Envelope destination for all the headers
 * @param extra Envelope to copy from
 *
 * Any fields that are missing from base will be copied from extra.
 * extra will be freed afterwards.
 */
void mutt_env_merge(struct Envelope *base, struct Envelope **extra)
{
/* copies each existing element if necessary, and sets the element
 * to NULL in the source so that mutt_env_free doesn't leave us
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

  mutt_env_free(extra);
}

/**
 * mutt_env_cmp_strict - Strictly compare two Envelopes
 * @param e1 First Envelope
 * @param e2 Second Envelope
 * @retval true Envelopes are strictly identical
 */
bool mutt_env_cmp_strict(const struct Envelope *e1, const struct Envelope *e2)
{
  if (e1 && e2)
  {
    if ((mutt_str_strcmp(e1->message_id, e2->message_id) != 0) ||
        (mutt_str_strcmp(e1->subject, e2->subject) != 0) ||
        !mutt_list_compare(&e1->references, &e2->references) ||
        !mutt_addr_cmp_strict(e1->from, e2->from) ||
        !mutt_addr_cmp_strict(e1->sender, e2->sender) ||
        !mutt_addr_cmp_strict(e1->reply_to, e2->reply_to) ||
        !mutt_addr_cmp_strict(e1->to, e2->to) || !mutt_addr_cmp_strict(e1->cc, e2->cc) ||
        !mutt_addr_cmp_strict(e1->return_path, e2->return_path))
      return false;
    else
      return true;
  }
  else
  {
    if (!e1 && !e2)
      return true;
    else
      return false;
  }
}

/**
 * mutt_env_to_local - Convert an Envelope's Address fields to local format
 * @param e Envelope to modify
 *
 * Run mutt_addrlist_to_local() on each of the Address fields in the Envelope.
 */
void mutt_env_to_local(struct Envelope *e)
{
  mutt_addrlist_to_local(e->return_path);
  mutt_addrlist_to_local(e->from);
  mutt_addrlist_to_local(e->to);
  mutt_addrlist_to_local(e->cc);
  mutt_addrlist_to_local(e->bcc);
  mutt_addrlist_to_local(e->reply_to);
  mutt_addrlist_to_local(e->mail_followup_to);
}

/* Note that `a' in the `env->a' expression is macro argument, not
 * "real" name of an `env' compound member.  Real name will be substituted
 * by preprocessor at the macro-expansion time.
 * Note that #a escapes and double quotes the argument.
 */
#define H_TO_INTL(a)                                                           \
  if (mutt_addrlist_to_intl(env->a, err) && !e)                                \
  {                                                                            \
    if (tag)                                                                   \
      *tag = #a;                                                               \
    e = 1;                                                                     \
    err = NULL;                                                                \
  }

/**
 * mutt_env_to_intl - Convert an Envelope's Address fields to Punycode format
 * @param[in]  env Envelope to modify
 * @param[out] tag Name of the failed field
 * @param[out] err Failed address
 * @retval 0 Success, all addresses converted
 * @retval 1 Error, tag and err will be set
 *
 * Run mutt_addrlist_to_intl() on each of the Address fields in the Envelope.
 */
int mutt_env_to_intl(struct Envelope *env, char **tag, char **err)
{
  int e = 0;
  H_TO_INTL(return_path);
  H_TO_INTL(from);
  H_TO_INTL(to);
  H_TO_INTL(cc);
  H_TO_INTL(bcc);
  H_TO_INTL(reply_to);
  H_TO_INTL(mail_followup_to);
  return e;
}

#undef H_TO_INTL
