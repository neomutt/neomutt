/**
 * @file
 * Email sorting methods
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019-2021 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
 * Copyright (C) 2021 Eric Blake <eblake@redhat.com>
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
 * @page email_sort Email sorting methods
 *
 * Email sorting methods
 */

#include "config.h"
#include <stdbool.h>
#include <stdlib.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "sort.h"
#include "nntp/lib.h"
#include "body.h"
#include "email.h"
#include "envelope.h"
#include "mutt_logging.h"
#include "mx.h"
#include "score.h"

extern bool OptNeedRescore;
extern bool OptNeedResort;
extern bool OptResortInit;

/**
 * struct EmailCompare - Context for email_sort_shim()
 */
struct EmailCompare
{
  enum MailboxType type; ///< Current mailbox type
  short sort;            ///< Primary sort
  short sort_aux;        ///< Secondary sort
};

/**
 * email_sort_shim - Helper to sort emails - Implements ::sort_t - @ingroup sort_api
 */
static int email_sort_shim(const void *a, const void *b, void *sdata)
{
  const struct Email *ea = *(struct Email const *const *) a;
  const struct Email *eb = *(struct Email const *const *) b;
  const struct EmailCompare *cmp = sdata;
  return mutt_compare_emails(ea, eb, cmp->type, cmp->sort, cmp->sort_aux);
}

/**
 * email_sort_score - Compare two emails using their scores - Implements ::sort_email_t - @ingroup sort_email_api
 */
static int email_sort_score(const struct Email *a, const struct Email *b, bool reverse)
{
  int result = mutt_numeric_cmp(b->score, a->score); /* note that this is reverse */
  return reverse ? -result : result;
}

/**
 * email_sort_size - Compare the size of two emails - Implements ::sort_email_t - @ingroup sort_email_api
 */
static int email_sort_size(const struct Email *a, const struct Email *b, bool reverse)
{
  int result = mutt_numeric_cmp(a->body->length, b->body->length);
  return reverse ? -result : result;
}

/**
 * email_sort_date - Compare the sent date of two emails - Implements ::sort_email_t - @ingroup sort_email_api
 */
static int email_sort_date(const struct Email *a, const struct Email *b, bool reverse)
{
  int result = mutt_numeric_cmp(a->date_sent, b->date_sent);
  return reverse ? -result : result;
}

/**
 * email_sort_subject - Compare the subject of two emails - Implements ::sort_email_t - @ingroup sort_email_api
 */
static int email_sort_subject(const struct Email *a, const struct Email *b, bool reverse)
{
  int rc;

  if (!a->env->real_subj)
  {
    if (!b->env->real_subj)
      rc = email_sort_date(a, b, false);
    else
      rc = -1;
  }
  else if (!b->env->real_subj)
  {
    rc = 1;
  }
  else
  {
    rc = mutt_istr_cmp(a->env->real_subj, b->env->real_subj);
  }
  return reverse ? -rc : rc;
}

/**
 * mutt_get_name - Pick the best name to display from an address
 * @param a Address to use
 * @retval ptr Display name
 *
 * This function uses:
 * 1. Alias for email address
 * 2. Personal name
 * 3. Email address
 */
const char *mutt_get_name(const struct Address *a)
{
  struct Address *ali = NULL;

  if (a)
  {
    const bool c_reverse_alias = cs_subset_bool(NeoMutt->sub, "reverse_alias");
    if (c_reverse_alias && (ali = alias_reverse_lookup(a)) && ali->personal)
      return buf_string(ali->personal);
    if (a->personal)
      return buf_string(a->personal);
    if (a->mailbox)
      return mutt_addr_for_display(a);
  }
  /* don't return NULL to avoid segfault when printing/comparing */
  return "";
}

/**
 * email_sort_to - Compare the 'to' fields of two emails - Implements ::sort_email_t - @ingroup sort_email_api
 */
static int email_sort_to(const struct Email *a, const struct Email *b, bool reverse)
{
  char fa[128] = { 0 };

  mutt_str_copy(fa, mutt_get_name(TAILQ_FIRST(&a->env->to)), sizeof(fa));
  const char *fb = mutt_get_name(TAILQ_FIRST(&b->env->to));
  int result = mutt_istrn_cmp(fa, fb, sizeof(fa));
  return reverse ? -result : result;
}

/**
 * email_sort_from - Compare the 'from' fields of two emails - Implements ::sort_email_t - @ingroup sort_email_api
 */
static int email_sort_from(const struct Email *a, const struct Email *b, bool reverse)
{
  char fa[128] = { 0 };

  mutt_str_copy(fa, mutt_get_name(TAILQ_FIRST(&a->env->from)), sizeof(fa));
  const char *fb = mutt_get_name(TAILQ_FIRST(&b->env->from));
  int result = mutt_istrn_cmp(fa, fb, sizeof(fa));
  return reverse ? -result : result;
}

/**
 * email_sort_date_received - Compare the date received of two emails - Implements ::sort_email_t - @ingroup sort_email_api
 */
static int email_sort_date_received(const struct Email *a, const struct Email *b, bool reverse)
{
  int result = mutt_numeric_cmp(a->received, b->received);
  return reverse ? -result : result;
}

/**
 * email_sort_unsorted - Restore the 'unsorted' order of emails - Implements ::sort_email_t - @ingroup sort_email_api
 */
static int email_sort_unsorted(const struct Email *a, const struct Email *b, bool reverse)
{
  int result = mutt_numeric_cmp(a->index, b->index);
  return reverse ? -result : result;
}

/**
 * email_sort_spam - Compare the spam values of two emails - Implements ::sort_email_t - @ingroup sort_email_api
 */
static int email_sort_spam(const struct Email *a, const struct Email *b, bool reverse)
{
  char *aptr = NULL, *bptr = NULL;
  int ahas, bhas;
  int result = 0;
  double difference;

  /* Firstly, require spam attributes for both msgs */
  /* to compare. Determine which msgs have one.     */
  ahas = a->env && !buf_is_empty(&a->env->spam);
  bhas = b->env && !buf_is_empty(&b->env->spam);

  /* If one msg has spam attr but other does not, sort the one with first. */
  if (ahas && !bhas)
    return reverse ? -1 : 1;
  if (!ahas && bhas)
    return reverse ? 1 : -1;

  /* Else, if neither has a spam attr, presume equality. Fall back on aux. */
  if (!ahas && !bhas)
    return 0;

  /* Both have spam attrs. */

  /* preliminary numeric examination */
  difference = (strtod(a->env->spam.data, &aptr) - strtod(b->env->spam.data, &bptr));

  /* map double into comparison (-1, 0, or 1) */
  result = ((difference < 0.0) ? -1 : (difference > 0.0) ? 1 : 0);

  /* If either aptr or bptr is equal to data, there is no numeric    */
  /* value for that spam attribute. In this case, compare lexically. */
  if ((aptr == a->env->spam.data) || (bptr == b->env->spam.data))
  {
    result = mutt_str_cmp(aptr, bptr);
    return reverse ? -result : result;
  }

  /* Otherwise, we have numeric value for both attrs. If these values */
  /* are equal, then we first fall back upon string comparison, then  */
  /* upon auxiliary sort.                                             */
  if (result == 0)
    result = mutt_str_cmp(aptr, bptr);
  return reverse ? -result : result;
}

/**
 * email_sort_label - Compare the labels of two emails - Implements ::sort_email_t - @ingroup sort_email_api
 */
static int email_sort_label(const struct Email *a, const struct Email *b, bool reverse)
{
  int ahas, bhas, result = 0;

  /* As with email_sort_spam, not all messages will have the x-label
   * property.  Blank X-Labels are treated as null in the index
   * display, so we'll consider them as null for sort, too.       */
  ahas = a->env && a->env->x_label && *(a->env->x_label);
  bhas = b->env && b->env->x_label && *(b->env->x_label);

  /* First we bias toward a message with a label, if the other does not. */
  if (ahas && !bhas)
    return reverse ? 1 : -1;
  if (!ahas && bhas)
    return reverse ? -1 : 1;

  /* If neither has a label, use aux sort. */
  if (!ahas && !bhas)
    return 0;

  /* If both have a label, we just do a lexical compare. */
  result = mutt_istr_cmp(a->env->x_label, b->env->x_label);
  return reverse ? -result : result;
}

/**
 * get_sort_func - Get the sort function for a given sort id
 * @param method Sort type, see #EmailSortType
 * @param type   The Mailbox type
 * @retval ptr sort function - Implements ::sort_email_t
 */
static sort_email_t get_sort_func(enum EmailSortType method, enum MailboxType type)
{
  switch (method)
  {
    case EMAIL_SORT_DATE:
      return email_sort_date;
    case EMAIL_SORT_DATE_RECEIVED:
      return email_sort_date_received;
    case EMAIL_SORT_FROM:
      return email_sort_from;
    case EMAIL_SORT_LABEL:
      return email_sort_label;
    case EMAIL_SORT_SCORE:
      return email_sort_score;
    case EMAIL_SORT_SIZE:
      return email_sort_size;
    case EMAIL_SORT_SPAM:
      return email_sort_spam;
    case EMAIL_SORT_SUBJECT:
      return email_sort_subject;
    case EMAIL_SORT_TO:
      return email_sort_to;
    case EMAIL_SORT_UNSORTED:
      if (type == MUTT_NNTP)
        return nntp_sort_unsorted;
      else
        return email_sort_unsorted;
    default:
      mutt_error(_("Could not find sorting function [report this bug]"));
      return NULL;
  }
  /* not reached */
}

/**
 * mutt_compare_emails - Compare two emails using up to two sort methods - @ingroup sort_api
 * @param a        First email
 * @param b        Second email
 * @param type     Mailbox type
 * @param sort     Primary sort to use (generally $sort)
 * @param sort_aux Secondary sort (generally $sort_aux or EMAIL_SORT_UNSORTED)
 * @retval <0 a precedes b
 * @retval  0 a and b are identical (should not happen in practice)
 * @retval >0 b precedes a
 */
int mutt_compare_emails(const struct Email *a, const struct Email *b,
                        enum MailboxType type, short sort, short sort_aux)
{
  sort_email_t func = get_sort_func(sort & SORT_MASK, type);
  int rc = func(a, b, (sort & SORT_REVERSE) != 0);
  if (rc == 0)
  {
    func = get_sort_func(sort_aux & SORT_MASK, type);
    rc = func(a, b, (sort_aux & SORT_REVERSE) != 0);
  }
  if (rc == 0)
  {
    /* Fallback of last resort to preserve stable order; will only
     * return 0 if a and b have the same index, which is probably a
     * bug in the code. */
    func = email_sort_unsorted;
    rc = func(a, b, false);
  }
  return rc;
}

/**
 * mutt_sort_headers - Sort emails by their headers
 * @param mv    Mailbox View
 * @param init  If true, rebuild the thread
 */
void mutt_sort_headers(struct MailboxView *mv, bool init)
{
  if (!mv)
    return;

  struct Mailbox *m = mv->mailbox;
  if (!m || !m->emails[0])
    return;

  OptNeedResort = false;

  if (m->msg_count == 0)
  {
    /* this function gets called by mutt_sync_mailbox(), which may have just
     * deleted all the messages.  the virtual message numbers are not updated
     * in that routine, so we must make sure to zero the vcount member.  */
    m->vcount = 0;
    mutt_clear_threads(mv->threads);
    mv->vsize = 0;
    return; /* nothing to do! */
  }

  if (m->verbose)
    mutt_message(_("Sorting mailbox..."));

  const bool c_score = cs_subset_bool(NeoMutt->sub, "score");
  if (OptNeedRescore && c_score)
  {
    for (int i = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;
      mutt_score_message(m, e, true);
    }
  }
  OptNeedRescore = false;

  if (OptResortInit)
  {
    OptResortInit = false;
    init = true;
  }

  if (init)
    mutt_clear_threads(mv->threads);

  const bool threaded = mutt_using_threads();
  if (threaded)
  {
    mutt_sort_threads(mv->threads, init);
  }
  else
  {
    struct EmailCompare cmp = { 0 };
    cmp.type = mx_type(m);
    cmp.sort = cs_subset_sort(NeoMutt->sub, "sort");
    cmp.sort_aux = cs_subset_sort(NeoMutt->sub, "sort_aux");
    mutt_qsort_r((void *) m->emails, m->msg_count, sizeof(struct Email *),
                 email_sort_shim, &cmp);
  }

  /* adjust the virtual message numbers */
  m->vcount = 0;
  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e_cur = m->emails[i];
    if (!e_cur)
      break;

    if ((e_cur->vnum != -1) || (e_cur->collapsed && e_cur->visible))
    {
      e_cur->vnum = m->vcount;
      m->v2r[m->vcount] = i;
      m->vcount++;
    }
    e_cur->msgno = i;
  }

  /* re-collapse threads marked as collapsed */
  if (threaded)
  {
    mutt_thread_collapse_collapsed(mv->threads);
    mv->vsize = mutt_set_vnum(m);
  }

  if (m->verbose)
    mutt_clear_error();
}

/**
 * mutt_sort_unsorted - Sort emails by their disk order
 * @param m Mailbox
 */
void mutt_sort_unsorted(struct Mailbox *m)
{
  if (!m)
    return;

  struct EmailCompare cmp = { 0 };
  cmp.type = mx_type(m);
  cmp.sort = EMAIL_SORT_UNSORTED;
  mutt_qsort_r((void *) m->emails, m->msg_count, sizeof(struct Email *),
               email_sort_shim, &cmp);
}
