/**
 * @file
 * Assorted sorting methods
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
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
 * @page neo_sort Assorted sorting methods
 *
 * Assorted sorting methods
 */

#include "config.h"
#include <stdbool.h>
#include <stdlib.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "sort.h"
#include "globals.h" // IWYU pragma: keep
#include "mutt_logging.h"
#include "mutt_thread.h"
#include "mx.h"
#include "score.h"
#ifdef USE_NNTP
#include "nntp/lib.h"
#endif

/**
 * struct EmailCompare - Context for compare_email_shim()
 */
struct EmailCompare
{
  enum MailboxType type; ///< Current mailbox type
  short sort;            ///< Primary sort
  short sort_aux;        ///< Secondary sort
};

/**
 * compare_email_shim - qsort_r() comparator to drive mutt_compare_emails
 * @param a   Pointer to first email
 * @param b   Pointer to second email
 * @param arg EmailCompare with needed context
 * @retval <0 a precedes b
 * @retval  0 a identical to b (should not happen in practice)
 * @retval >0 b precedes a
 */
static int compare_email_shim(const void *a, const void *b, void *arg)
{
  const struct Email *ea = *(struct Email const *const *) a;
  const struct Email *eb = *(struct Email const *const *) b;
  const struct EmailCompare *cmp = arg;
  return mutt_compare_emails(ea, eb, cmp->type, cmp->sort, cmp->sort_aux);
}

/**
 * compare_score - Compare two emails using their scores - Implements ::sort_mail_t - @ingroup sort_mail_api
 */
static int compare_score(const struct Email *a, const struct Email *b, bool reverse)
{
  int result = mutt_numeric_cmp(b->score, a->score); /* note that this is reverse */
  return reverse ? -result : result;
}

/**
 * compare_size - Compare the size of two emails - Implements ::sort_mail_t - @ingroup sort_mail_api
 */
static int compare_size(const struct Email *a, const struct Email *b, bool reverse)
{
  int result = mutt_numeric_cmp(a->body->length, b->body->length);
  return reverse ? -result : result;
}

/**
 * compare_date_sent - Compare the sent date of two emails - Implements ::sort_mail_t - @ingroup sort_mail_api
 */
static int compare_date_sent(const struct Email *a, const struct Email *b, bool reverse)
{
  int result = mutt_numeric_cmp(a->date_sent, b->date_sent);
  return reverse ? -result : result;
}

/**
 * compare_subject - Compare the subject of two emails - Implements ::sort_mail_t - @ingroup sort_mail_api
 */
static int compare_subject(const struct Email *a, const struct Email *b, bool reverse)
{
  int rc;

  if (!a->env->real_subj)
  {
    if (!b->env->real_subj)
      rc = compare_date_sent(a, b, false);
    else
      rc = -1;
  }
  else if (!b->env->real_subj)
    rc = 1;
  else
    rc = mutt_istr_cmp(a->env->real_subj, b->env->real_subj);
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
      return ali->personal;
    if (a->personal)
      return a->personal;
    if (a->mailbox)
      return mutt_addr_for_display(a);
  }
  /* don't return NULL to avoid segfault when printing/comparing */
  return "";
}

/**
 * compare_to - Compare the 'to' fields of two emails - Implements ::sort_mail_t - @ingroup sort_mail_api
 */
static int compare_to(const struct Email *a, const struct Email *b, bool reverse)
{
  char fa[128] = { 0 };

  mutt_str_copy(fa, mutt_get_name(TAILQ_FIRST(&a->env->to)), sizeof(fa));
  const char *fb = mutt_get_name(TAILQ_FIRST(&b->env->to));
  int result = mutt_istrn_cmp(fa, fb, sizeof(fa));
  return reverse ? -result : result;
}

/**
 * compare_from - Compare the 'from' fields of two emails - Implements ::sort_mail_t - @ingroup sort_mail_api
 */
static int compare_from(const struct Email *a, const struct Email *b, bool reverse)
{
  char fa[128] = { 0 };

  mutt_str_copy(fa, mutt_get_name(TAILQ_FIRST(&a->env->from)), sizeof(fa));
  const char *fb = mutt_get_name(TAILQ_FIRST(&b->env->from));
  int result = mutt_istrn_cmp(fa, fb, sizeof(fa));
  return reverse ? -result : result;
}

/**
 * compare_date_received - Compare the date received of two emails - Implements ::sort_mail_t - @ingroup sort_mail_api
 */
static int compare_date_received(const struct Email *a, const struct Email *b, bool reverse)
{
  int result = mutt_numeric_cmp(a->received, b->received);
  return reverse ? -result : result;
}

/**
 * compare_order - Restore the 'unsorted' order of emails - Implements ::sort_mail_t - @ingroup sort_mail_api
 */
static int compare_order(const struct Email *a, const struct Email *b, bool reverse)
{
  int result = mutt_numeric_cmp(a->index, b->index);
  return reverse ? -result : result;
}

/**
 * compare_spam - Compare the spam values of two emails - Implements ::sort_mail_t - @ingroup sort_mail_api
 */
static int compare_spam(const struct Email *a, const struct Email *b, bool reverse)
{
  char *aptr = NULL, *bptr = NULL;
  int ahas, bhas;
  int result = 0;
  double difference;

  /* Firstly, require spam attributes for both msgs */
  /* to compare. Determine which msgs have one.     */
  ahas = a->env && !mutt_buffer_is_empty(&a->env->spam);
  bhas = b->env && !mutt_buffer_is_empty(&b->env->spam);

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
 * compare_label - Compare the labels of two emails - Implements ::sort_mail_t - @ingroup sort_mail_api
 */
static int compare_label(const struct Email *a, const struct Email *b, bool reverse)
{
  int ahas, bhas, result = 0;

  /* As with compare_spam, not all messages will have the x-label
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
 * @param method Sort type, see #SortType
 * @param type   The Mailbox type
 * @retval ptr sort function - Implements ::sort_mail_t
 */
static sort_mail_t get_sort_func(enum SortType method, enum MailboxType type)
{
  switch (method)
  {
    case SORT_DATE:
      return compare_date_sent;
    case SORT_FROM:
      return compare_from;
    case SORT_LABEL:
      return compare_label;
    case SORT_ORDER:
#ifdef USE_NNTP
      if (type == MUTT_NNTP)
        return nntp_compare_order;
      else
#endif
        return compare_order;
    case SORT_RECEIVED:
      return compare_date_received;
    case SORT_SCORE:
      return compare_score;
    case SORT_SIZE:
      return compare_size;
    case SORT_SPAM:
      return compare_spam;
    case SORT_SUBJECT:
      return compare_subject;
    case SORT_TO:
      return compare_to;
    default:
      mutt_error(_("Could not find sorting function [report this bug]"));
      return NULL;
  }
  /* not reached */
}

/**
 * mutt_compare_emails - Compare two emails using up to two sort methods
 * @param a        First email
 * @param b        Second email
 * @param type     Mailbox type
 * @param sort     Primary sort to use (generally $sort)
 * @param sort_aux Secondary sort (generally $sort_aux or SORT_ORDER)
 * @retval <0 a precedes b
 * @retval  0 a and b are identical (should not happen in practice)
 * @retval >0 b precedes a
 */
int mutt_compare_emails(const struct Email *a, const struct Email *b,
                        enum MailboxType type, short sort, short sort_aux)
{
  sort_mail_t func = get_sort_func(sort & SORT_MASK, type);
  int retval = func(a, b, (sort & SORT_REVERSE) != 0);
  if (retval == 0)
  {
    func = get_sort_func(sort_aux & SORT_MASK, type);
    retval = func(a, b, (sort_aux & SORT_REVERSE) != 0);
  }
  if (retval == 0)
  {
    /* Fallback of last resort to preserve stable order; will only
     * return 0 if a and b have the same index, which is probably a
     * bug in the code. */
    func = compare_order;
    retval = func(a, b, false);
  }
  return retval;
}

/**
 * mutt_sort_headers - Sort emails by their headers
 * @param m       Mailbox
 * @param threads Threads context
 * @param init If true, rebuild the thread
 * @param[out] vsize Size in bytes of the messages in view
 */
void mutt_sort_headers(struct Mailbox *m, struct ThreadsContext *threads,
                       bool init, off_t *vsize)
{
  if (!m || !m->emails[0])
    return;

  OptNeedResort = false;

  if (m->msg_count == 0)
  {
    /* this function gets called by mutt_sync_mailbox(), which may have just
     * deleted all the messages.  the virtual message numbers are not updated
     * in that routine, so we must make sure to zero the vcount member.  */
    m->vcount = 0;
    mutt_clear_threads(threads);
    *vsize = 0;
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
    mutt_clear_threads(threads);

  const bool threaded = mutt_using_threads();
  if (threaded)
  {
    mutt_sort_threads(threads, init);
  }
  else
  {
    struct EmailCompare cmp;
    cmp.type = mx_type(m);
    cmp.sort = cs_subset_sort(NeoMutt->sub, "sort");
    cmp.sort_aux = cs_subset_sort(NeoMutt->sub, "sort_aux");
    mutt_qsort_r((void *) m->emails, m->msg_count, sizeof(struct Email *),
                 compare_email_shim, &cmp);
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
    mutt_thread_collapse_collapsed(threads);
    *vsize = mutt_set_vnum(m);
  }

  if (m->verbose)
    mutt_clear_error();

  return;
}
