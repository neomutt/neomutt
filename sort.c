/**
 * @file
 * Assorted sorting methods
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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
 * @page sort Assorted sorting methods
 *
 * Assorted sorting methods
 */

#include "config.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "sort.h"
#include "context.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_thread.h"
#include "options.h"
#include "score.h"
#ifdef USE_NNTP
#include "nntp/lib.h"
#endif

/* These Config Variables are only used in sort.c */
bool C_ReverseAlias; ///< Config: Display the alias in the index, rather than the message's sender

/* function to use as discriminator when normal sort method is equal */
static sort_t AuxSort = NULL;

/**
 * perform_auxsort - Compare two emails using the auxiliary sort method
 * @param retval Result of normal sort method
 * @param a      First email
 * @param b      Second email
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int perform_auxsort(int retval, const void *a, const void *b)
{
  /* If the items compared equal by the main sort
   * and we're not already doing an 'aux' sort...  */
  if ((retval == 0) && AuxSort && !OptAuxSort)
  {
    OptAuxSort = true;
    retval = AuxSort(a, b);
    OptAuxSort = false;
    if (retval != 0)
      return retval;
  }
  /* If the items still match, use their index positions
   * to maintain a stable sort order */
  if (retval == 0)
  {
    retval = (*((struct Email const *const *) a))->index -
             (*((struct Email const *const *) b))->index;
  }
  return retval;
}

/**
 * compare_score - Compare two emails using their scores - Implements ::sort_t
 */
static int compare_score(const void *a, const void *b)
{
  struct Email const *const *pa = (struct Email const *const *) a;
  struct Email const *const *pb = (struct Email const *const *) b;
  int result = (*pb)->score - (*pa)->score; /* note that this is reverse */
  result = perform_auxsort(result, a, b);
  return SORT_CODE(result);
}

/**
 * compare_size - Compare the size of two emails - Implements ::sort_t
 */
static int compare_size(const void *a, const void *b)
{
  struct Email const *const *pa = (struct Email const *const *) a;
  struct Email const *const *pb = (struct Email const *const *) b;
  int result = (*pa)->content->length - (*pb)->content->length;
  result = perform_auxsort(result, a, b);
  return SORT_CODE(result);
}

/**
 * compare_date_sent - Compare the sent date of two emails - Implements ::sort_t
 */
static int compare_date_sent(const void *a, const void *b)
{
  struct Email const *const *pa = (struct Email const *const *) a;
  struct Email const *const *pb = (struct Email const *const *) b;
  int result = (*pa)->date_sent - (*pb)->date_sent;
  result = perform_auxsort(result, a, b);
  return SORT_CODE(result);
}

/**
 * compare_subject - Compare the subject of two emails - Implements ::sort_t
 */
static int compare_subject(const void *a, const void *b)
{
  struct Email const *const *pa = (struct Email const *const *) a;
  struct Email const *const *pb = (struct Email const *const *) b;
  int rc;

  if (!(*pa)->env->real_subj)
  {
    if (!(*pb)->env->real_subj)
      rc = compare_date_sent(pa, pb);
    else
      rc = -1;
  }
  else if (!(*pb)->env->real_subj)
    rc = 1;
  else
    rc = mutt_istr_cmp((*pa)->env->real_subj, (*pb)->env->real_subj);
  rc = perform_auxsort(rc, a, b);
  return SORT_CODE(rc);
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
    if (C_ReverseAlias && (ali = alias_reverse_lookup(a)) && ali->personal)
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
 * compare_to - Compare the 'to' fields of two emails - Implements ::sort_t
 */
static int compare_to(const void *a, const void *b)
{
  struct Email const *const *ppa = (struct Email const *const *) a;
  struct Email const *const *ppb = (struct Email const *const *) b;
  char fa[128];

  mutt_str_copy(fa, mutt_get_name(TAILQ_FIRST(&(*ppa)->env->to)), sizeof(fa));
  const char *fb = mutt_get_name(TAILQ_FIRST(&(*ppb)->env->to));
  int result = mutt_istrn_cmp(fa, fb, sizeof(fa));
  result = perform_auxsort(result, a, b);
  return SORT_CODE(result);
}

/**
 * compare_from - Compare the 'from' fields of two emails - Implements ::sort_t
 */
static int compare_from(const void *a, const void *b)
{
  struct Email const *const *ppa = (struct Email const *const *) a;
  struct Email const *const *ppb = (struct Email const *const *) b;
  char fa[128];

  mutt_str_copy(fa, mutt_get_name(TAILQ_FIRST(&(*ppa)->env->from)), sizeof(fa));
  const char *fb = mutt_get_name(TAILQ_FIRST(&(*ppb)->env->from));
  int result = mutt_istrn_cmp(fa, fb, sizeof(fa));
  result = perform_auxsort(result, a, b);
  return SORT_CODE(result);
}

/**
 * compare_date_received - Compare the date received of two emails - Implements ::sort_t
 */
static int compare_date_received(const void *a, const void *b)
{
  struct Email const *const *pa = (struct Email const *const *) a;
  struct Email const *const *pb = (struct Email const *const *) b;
  int result = (*pa)->received - (*pb)->received;
  result = perform_auxsort(result, a, b);
  return SORT_CODE(result);
}

/**
 * compare_order - Restore the 'unsorted' order of emails - Implements ::sort_t
 */
static int compare_order(const void *a, const void *b)
{
  struct Email const *const *ea = (struct Email const *const *) a;
  struct Email const *const *eb = (struct Email const *const *) b;

  /* no need to auxsort because you will never have equality here */
  return SORT_CODE((*ea)->index - (*eb)->index);
}

/**
 * compare_spam - Compare the spam values of two emails - Implements ::sort_t
 */
static int compare_spam(const void *a, const void *b)
{
  struct Email const *const *ppa = (struct Email const *const *) a;
  struct Email const *const *ppb = (struct Email const *const *) b;
  char *aptr = NULL, *bptr = NULL;
  int ahas, bhas;
  int result = 0;
  double difference;

  /* Firstly, require spam attributes for both msgs */
  /* to compare. Determine which msgs have one.     */
  ahas = (*ppa)->env && !mutt_buffer_is_empty(&(*ppa)->env->spam);
  bhas = (*ppb)->env && !mutt_buffer_is_empty(&(*ppb)->env->spam);

  /* If one msg has spam attr but other does not, sort the one with first. */
  if (ahas && !bhas)
    return SORT_CODE(1);
  if (!ahas && bhas)
    return SORT_CODE(-1);

  /* Else, if neither has a spam attr, presume equality. Fall back on aux. */
  if (!ahas && !bhas)
  {
    result = perform_auxsort(result, a, b);
    return SORT_CODE(result);
  }

  /* Both have spam attrs. */

  /* preliminary numeric examination */
  difference =
      (strtod((*ppa)->env->spam.data, &aptr) - strtod((*ppb)->env->spam.data, &bptr));

  /* map double into comparison (-1, 0, or 1) */
  result = ((difference < 0.0) ? -1 : (difference > 0.0) ? 1 : 0);

  /* If either aptr or bptr is equal to data, there is no numeric    */
  /* value for that spam attribute. In this case, compare lexically. */
  if ((aptr == (*ppa)->env->spam.data) || (bptr == (*ppb)->env->spam.data))
    return SORT_CODE(strcmp(aptr, bptr));

  /* Otherwise, we have numeric value for both attrs. If these values */
  /* are equal, then we first fall back upon string comparison, then  */
  /* upon auxiliary sort.                                             */
  if (result == 0)
  {
    result = strcmp(aptr, bptr);
    result = perform_auxsort(result, a, b);
  }

  return SORT_CODE(result);
}

/**
 * compare_label - Compare the labels of two emails - Implements ::sort_t
 */
static int compare_label(const void *a, const void *b)
{
  struct Email const *const *ppa = (struct Email const *const *) a;
  struct Email const *const *ppb = (struct Email const *const *) b;
  int ahas, bhas, result = 0;

  /* As with compare_spam, not all messages will have the x-label
   * property.  Blank X-Labels are treated as null in the index
   * display, so we'll consider them as null for sort, too.       */
  ahas = (*ppa)->env && (*ppa)->env->x_label && *((*ppa)->env->x_label);
  bhas = (*ppb)->env && (*ppb)->env->x_label && *((*ppb)->env->x_label);

  /* First we bias toward a message with a label, if the other does not. */
  if (ahas && !bhas)
    return SORT_CODE(-1);
  if (!ahas && bhas)
    return SORT_CODE(1);

  /* If neither has a label, use aux sort. */
  if (!ahas && !bhas)
  {
    result = perform_auxsort(result, a, b);
    return SORT_CODE(result);
  }

  /* If both have a label, we just do a lexical compare. */
  result = mutt_istr_cmp((*ppa)->env->x_label, (*ppb)->env->x_label);
  return SORT_CODE(result);
}

/**
 * mutt_get_sort_func - Get the sort function for a given sort id
 * @param method Sort type, see #SortType
 * @retval ptr sort function - Implements ::sort_t
 */
sort_t mutt_get_sort_func(enum SortType method)
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
      if (Context && Context->mailbox && (Context->mailbox->type == MUTT_NNTP))
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
      return NULL;
  }
  /* not reached */
}

/**
 * mutt_sort_headers - Sort emails by their headers
 * @param ctx  Mailbox
 * @param init If true, rebuild the thread
 */
void mutt_sort_headers(struct Context *ctx, bool init)
{
  if (!ctx || !ctx->mailbox || !ctx->mailbox->emails || !ctx->mailbox->emails[0])
    return;

  struct MuttThread *thread = NULL, *top = NULL;
  sort_t sortfunc = NULL;
  struct Mailbox *m = ctx->mailbox;

  OptNeedResort = false;

  if (m->msg_count == 0)
  {
    /* this function gets called by mutt_sync_mailbox(), which may have just
     * deleted all the messages.  the virtual message numbers are not updated
     * in that routine, so we must make sure to zero the vcount member.  */
    m->vcount = 0;
    ctx->vsize = 0;
    mutt_clear_threads(ctx);
    return; /* nothing to do! */
  }

  if (m->verbose)
    mutt_message(_("Sorting mailbox..."));

  if (OptNeedRescore && C_Score)
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

  if (init && ctx->tree)
    mutt_clear_threads(ctx);

  if ((C_Sort & SORT_MASK) == SORT_THREADS)
  {
    AuxSort = NULL;
    /* if $sort_aux changed after the mailbox is sorted, then all the
     * subthreads need to be resorted */
    if (OptSortSubthreads)
    {
      int i = C_Sort;
      C_Sort = C_SortAux;
      if (ctx->tree)
        ctx->tree = mutt_sort_subthreads(ctx->tree, true);
      C_Sort = i;
      OptSortSubthreads = false;
    }
    mutt_sort_threads(ctx, init);
  }
  else if (!(sortfunc = mutt_get_sort_func(C_Sort & SORT_MASK)) ||
           !(AuxSort = mutt_get_sort_func(C_SortAux & SORT_MASK)))
  {
    mutt_error(_("Could not find sorting function [report this bug]"));
    return;
  }
  else
  {
    qsort((void *) m->emails, m->msg_count, sizeof(struct Email *), sortfunc);
  }

  /* adjust the virtual message numbers */
  m->vcount = 0;
  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e_cur = m->emails[i];
    if (!e_cur)
      break;

    if ((e_cur->vnum != -1) || (e_cur->collapsed && (!ctx->pattern || e_cur->limited)))
    {
      e_cur->vnum = m->vcount;
      m->v2r[m->vcount] = i;
      m->vcount++;
    }
    e_cur->msgno = i;
  }

  /* re-collapse threads marked as collapsed */
  if ((C_Sort & SORT_MASK) == SORT_THREADS)
  {
    top = ctx->tree;
    while ((thread = top))
    {
      while (!thread->message)
        thread = thread->child;

      struct Email *e = thread->message;
      if (e->collapsed)
        mutt_collapse_thread(ctx, e);
      top = top->next;
    }
    mutt_set_vnum(ctx);
  }

  if (m->verbose)
    mutt_clear_error();
}
