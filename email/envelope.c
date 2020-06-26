/**
 * @file
 * Representation of an email header (envelope)
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page email_envelope Representation of an email header (envelope)
 *
 * Representation of an email header (envelope)
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "envelope.h"

/**
 * mutt_env_new - Create a new Envelope
 * @retval ptr New Envelope
 */
struct Envelope *mutt_env_new(void)
{
  struct Envelope *e = mutt_mem_calloc(1, sizeof(struct Envelope));
  TAILQ_INIT(&e->return_path);
  TAILQ_INIT(&e->from);
  TAILQ_INIT(&e->to);
  TAILQ_INIT(&e->cc);
  TAILQ_INIT(&e->bcc);
  TAILQ_INIT(&e->sender);
  TAILQ_INIT(&e->reply_to);
  TAILQ_INIT(&e->mail_followup_to);
  TAILQ_INIT(&e->x_original_to);
  STAILQ_INIT(&e->references);
  STAILQ_INIT(&e->in_reply_to);
  STAILQ_INIT(&e->userhdrs);
  return e;
}

#ifdef USE_AUTOCRYPT
/**
 * mutt_autocrypthdr_new - Create a new AutocryptHeader
 * @retval ptr New AutocryptHeader
 */
struct AutocryptHeader *mutt_autocrypthdr_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct AutocryptHeader));
}

/**
 * mutt_autocrypthdr_free - Free an AutocryptHeader
 * @param p AutocryptHeader to free
 */
void mutt_autocrypthdr_free(struct AutocryptHeader **p)
{
  if (!p)
    return;

  struct AutocryptHeader *cur = NULL;

  while (*p)
  {
    cur = *p;
    *p = (*p)->next;
    FREE(&cur->addr);
    FREE(&cur->keydata);
    FREE(&cur);
  }
}
#endif

/**
 * mutt_env_free - Free an Envelope
 * @param[out] ptr Envelope to free
 */
void mutt_env_free(struct Envelope **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Envelope *env = *ptr;

  mutt_addrlist_clear(&env->return_path);
  mutt_addrlist_clear(&env->from);
  mutt_addrlist_clear(&env->to);
  mutt_addrlist_clear(&env->cc);
  mutt_addrlist_clear(&env->bcc);
  mutt_addrlist_clear(&env->sender);
  mutt_addrlist_clear(&env->reply_to);
  mutt_addrlist_clear(&env->mail_followup_to);
  mutt_addrlist_clear(&env->x_original_to);

  FREE(&env->list_post);
  FREE(&env->subject);
  /* real_subj is just an offset to subject and shouldn't be freed */
  FREE(&env->disp_subj);
  FREE(&env->message_id);
  FREE(&env->supersedes);
  FREE(&env->date);
  FREE(&env->x_label);
  FREE(&env->organization);
#ifdef USE_NNTP
  FREE(&env->newsgroups);
  FREE(&env->xref);
  FREE(&env->followup_to);
  FREE(&env->x_comment_to);
#endif

  mutt_buffer_dealloc(&env->spam);

  mutt_list_free(&env->references);
  mutt_list_free(&env->in_reply_to);
  mutt_list_free(&env->userhdrs);

#ifdef USE_AUTOCRYPT
  mutt_autocrypthdr_free(&env->autocrypt);
  mutt_autocrypthdr_free(&env->autocrypt_gossip);
#endif

  FREE(ptr);
}

/**
 * mutt_env_merge - Merge the headers of two Envelopes
 * @param[in]  base  Envelope destination for all the headers
 * @param[out] extra Envelope to copy from
 *
 * Any fields that are missing from base will be copied from extra.
 * extra will be freed afterwards.
 */
void mutt_env_merge(struct Envelope *base, struct Envelope **extra)
{
  if (!base || !extra || !*extra)
    return;

/* copies each existing element if necessary, and sets the element
 * to NULL in the source so that mutt_env_free doesn't leave us
 * with dangling pointers. */
#define MOVE_ELEM(member)                                                      \
  if (!base->member)                                                           \
  {                                                                            \
    base->member = (*extra)->member;                                           \
    (*extra)->member = NULL;                                                   \
  }

#define MOVE_STAILQ(member)                                                    \
  if (STAILQ_EMPTY(&base->member))                                             \
  {                                                                            \
    STAILQ_SWAP(&base->member, &(*extra)->member, ListNode);                   \
  }

#define MOVE_ADDRESSLIST(member)                                               \
  if (TAILQ_EMPTY(&base->member))                                              \
  {                                                                            \
    TAILQ_SWAP(&base->member, &(*extra)->member, Address, entries);            \
  }

#define MOVE_BUFFER(member)                                                    \
  if (mutt_buffer_is_empty(&base->member))                                     \
  {                                                                            \
    memcpy(&base->member, &(*extra)->member, sizeof(struct Buffer));           \
    mutt_buffer_init(&(*extra)->member);                                       \
  }

  MOVE_ADDRESSLIST(return_path);
  MOVE_ADDRESSLIST(from);
  MOVE_ADDRESSLIST(to);
  MOVE_ADDRESSLIST(cc);
  MOVE_ADDRESSLIST(bcc);
  MOVE_ADDRESSLIST(sender);
  MOVE_ADDRESSLIST(reply_to);
  MOVE_ADDRESSLIST(mail_followup_to);
  MOVE_ELEM(list_post);
  MOVE_ELEM(message_id);
  MOVE_ELEM(supersedes);
  MOVE_ELEM(date);
  MOVE_ADDRESSLIST(x_original_to);
  if (!(base->changed & MUTT_ENV_CHANGED_XLABEL))
  {
    MOVE_ELEM(x_label);
  }
  if (!(base->changed & MUTT_ENV_CHANGED_REFS))
  {
    MOVE_STAILQ(references);
  }
  if (!(base->changed & MUTT_ENV_CHANGED_IRT))
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
  mutt_buffer_dealloc(&base->spam);
  mutt_list_free(&base->userhdrs);
  MOVE_BUFFER(spam);
  MOVE_STAILQ(userhdrs);
#undef MOVE_ELEM
#undef MOVE_STAILQ
#undef MOVE_ADDRESSLIST
#undef MOVE_BUFFER

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
    if (!mutt_str_equal(e1->message_id, e2->message_id) ||
        !mutt_str_equal(e1->subject, e2->subject) ||
        !mutt_list_compare(&e1->references, &e2->references) ||
        !mutt_addrlist_equal(&e1->from, &e2->from) ||
        !mutt_addrlist_equal(&e1->sender, &e2->sender) ||
        !mutt_addrlist_equal(&e1->reply_to, &e2->reply_to) ||
        !mutt_addrlist_equal(&e1->to, &e2->to) || !mutt_addrlist_equal(&e1->cc, &e2->cc) ||
        !mutt_addrlist_equal(&e1->return_path, &e2->return_path))
    {
      return false;
    }
    else
      return true;
  }
  else
  {
    return (!e1 && !e2);
  }
}

/**
 * mutt_env_to_local - Convert an Envelope's Address fields to local format
 * @param env Envelope to modify
 *
 * Run mutt_addrlist_to_local() on each of the Address fields in the Envelope.
 */
void mutt_env_to_local(struct Envelope *env)
{
  if (!env)
    return;

  mutt_addrlist_to_local(&env->return_path);
  mutt_addrlist_to_local(&env->from);
  mutt_addrlist_to_local(&env->to);
  mutt_addrlist_to_local(&env->cc);
  mutt_addrlist_to_local(&env->bcc);
  mutt_addrlist_to_local(&env->reply_to);
  mutt_addrlist_to_local(&env->mail_followup_to);
}

/* Note that 'member' in the 'env->member' expression is macro argument, not
 * "real" name of an 'env' compound member.  Real name will be substituted by
 * preprocessor at the macro-expansion time.
 * Note that #member escapes and double quotes the argument.
 */
#define H_TO_INTL(member)                                                      \
  if (mutt_addrlist_to_intl(&env->member, err) && !e)                          \
  {                                                                            \
    if (tag)                                                                   \
      *tag = #member;                                                          \
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
int mutt_env_to_intl(struct Envelope *env, const char **tag, char **err)
{
  if (!env)
    return 1;

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
