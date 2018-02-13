/**
 * @file
 * Assorted sorting methods
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/mutt.h"
#include "sort.h"
#include "body.h"
#include "context.h"
#include "envelope.h"
#include "globals.h"
#include "header.h"
#include "options.h"
#include "protos.h"
#include "thread.h"
#ifdef USE_NNTP
#include "mx.h"
#include "nntp.h"
#endif

#define SORTCODE(x) (Sort & SORT_REVERSE) ? -(x) : x

/* function to use as discriminator when normal sort method is equal */
static sort_t *AuxSort = NULL;

static int perform_auxsort(int retval, const void *a, const void *b)
{
  /* If the items compared equal by the main sort
   * and we're not already doing an 'aux' sort...  */
  if ((retval == 0) && AuxSort && !OPT_AUX_SORT)
  {
    OPT_AUX_SORT = true;
    retval = AuxSort(a, b);
    OPT_AUX_SORT = false;
  }
  /* If the items still match, use their index positions
   * to maintain a stable sort order */
  if (retval == 0)
    retval = (*((struct Header **) a))->index - (*((struct Header **) b))->index;
  return retval;
}

static int compare_score(const void *a, const void *b)
{
  struct Header **pa = (struct Header **) a;
  struct Header **pb = (struct Header **) b;
  int result = (*pb)->score - (*pa)->score; /* note that this is reverse */
  result = perform_auxsort(result, a, b);
  return (SORTCODE(result));
}

static int compare_size(const void *a, const void *b)
{
  struct Header **pa = (struct Header **) a;
  struct Header **pb = (struct Header **) b;
  int result = (*pa)->content->length - (*pb)->content->length;
  result = perform_auxsort(result, a, b);
  return (SORTCODE(result));
}

static int compare_date_sent(const void *a, const void *b)
{
  struct Header **pa = (struct Header **) a;
  struct Header **pb = (struct Header **) b;
  int result = (*pa)->date_sent - (*pb)->date_sent;
  result = perform_auxsort(result, a, b);
  return (SORTCODE(result));
}

static int compare_subject(const void *a, const void *b)
{
  struct Header **pa = (struct Header **) a;
  struct Header **pb = (struct Header **) b;
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
    rc = mutt_str_strcasecmp((*pa)->env->real_subj, (*pb)->env->real_subj);
  rc = perform_auxsort(rc, a, b);
  return (SORTCODE(rc));
}

const char *mutt_get_name(struct Address *a)
{
  struct Address *ali = NULL;

  if (a)
  {
    if (ReverseAlias && (ali = alias_reverse_lookup(a)) && ali->personal)
      return ali->personal;
    else if (a->personal)
      return a->personal;
    else if (a->mailbox)
      return (mutt_addr_for_display(a));
  }
  /* don't return NULL to avoid segfault when printing/comparing */
  return "";
}

static int compare_to(const void *a, const void *b)
{
  struct Header **ppa = (struct Header **) a;
  struct Header **ppb = (struct Header **) b;
  char fa[SHORT_STRING];
  const char *fb = NULL;
  int result;

  mutt_str_strfcpy(fa, mutt_get_name((*ppa)->env->to), SHORT_STRING);
  fb = mutt_get_name((*ppb)->env->to);
  result = mutt_str_strncasecmp(fa, fb, SHORT_STRING);
  result = perform_auxsort(result, a, b);
  return (SORTCODE(result));
}

static int compare_from(const void *a, const void *b)
{
  struct Header **ppa = (struct Header **) a;
  struct Header **ppb = (struct Header **) b;
  char fa[SHORT_STRING];
  const char *fb = NULL;
  int result;

  mutt_str_strfcpy(fa, mutt_get_name((*ppa)->env->from), SHORT_STRING);
  fb = mutt_get_name((*ppb)->env->from);
  result = mutt_str_strncasecmp(fa, fb, SHORT_STRING);
  result = perform_auxsort(result, a, b);
  return (SORTCODE(result));
}

static int compare_date_received(const void *a, const void *b)
{
  struct Header **pa = (struct Header **) a;
  struct Header **pb = (struct Header **) b;
  int result = (*pa)->received - (*pb)->received;
  result = perform_auxsort(result, a, b);
  return (SORTCODE(result));
}

static int compare_order(const void *a, const void *b)
{
  struct Header **ha = (struct Header **) a;
  struct Header **hb = (struct Header **) b;

#ifdef USE_NNTP
  if (Context && Context->magic == MUTT_NNTP)
  {
    anum_t na = NHDR(*ha)->article_num;
    anum_t nb = NHDR(*hb)->article_num;
    int result = na == nb ? 0 : na > nb ? 1 : -1;
    result = perform_auxsort(result, a, b);
    return (SORTCODE(result));
  }
  else
#endif
    /* no need to auxsort because you will never have equality here */
    return (SORTCODE((*ha)->index - (*hb)->index));
}

static int compare_spam(const void *a, const void *b)
{
  struct Header **ppa = (struct Header **) a;
  struct Header **ppb = (struct Header **) b;
  char *aptr = NULL, *bptr = NULL;
  int ahas, bhas;
  int result = 0;
  double difference;

  /* Firstly, require spam attributes for both msgs */
  /* to compare. Determine which msgs have one.     */
  ahas = (*ppa)->env && (*ppa)->env->spam;
  bhas = (*ppb)->env && (*ppb)->env->spam;

  /* If one msg has spam attr but other does not, sort the one with first. */
  if (ahas && !bhas)
    return (SORTCODE(1));
  if (!ahas && bhas)
    return (SORTCODE(-1));

  /* Else, if neither has a spam attr, presume equality. Fall back on aux. */
  if (!ahas && !bhas)
  {
    result = perform_auxsort(result, a, b);
    return (SORTCODE(result));
  }

  /* Both have spam attrs. */

  /* preliminary numeric examination */
  difference =
      (strtod((*ppa)->env->spam->data, &aptr) - strtod((*ppb)->env->spam->data, &bptr));

  /* map double into comparison (-1, 0, or 1) */
  result = (difference < 0.0 ? -1 : difference > 0.0 ? 1 : 0);

  /* If either aptr or bptr is equal to data, there is no numeric    */
  /* value for that spam attribute. In this case, compare lexically. */
  if ((aptr == (*ppa)->env->spam->data) || (bptr == (*ppb)->env->spam->data))
    return (SORTCODE(strcmp(aptr, bptr)));

  /* Otherwise, we have numeric value for both attrs. If these values */
  /* are equal, then we first fall back upon string comparison, then  */
  /* upon auxiliary sort.                                             */
  if (result == 0)
  {
    result = strcmp(aptr, bptr);
    if (result == 0)
    {
      result = perform_auxsort(result, a, b);
    }
  }

  return (SORTCODE(result));
}

static int compare_label(const void *a, const void *b)
{
  struct Header **ppa = (struct Header **) a;
  struct Header **ppb = (struct Header **) b;
  int ahas, bhas, result = 0;

  /* As with compare_spam, not all messages will have the x-label
   * property.  Blank X-Labels are treated as null in the index
   * display, so we'll consider them as null for sort, too.       */
  ahas = (*ppa)->env && (*ppa)->env->x_label && *((*ppa)->env->x_label);
  bhas = (*ppb)->env && (*ppb)->env->x_label && *((*ppb)->env->x_label);

  /* First we bias toward a message with a label, if the other does not. */
  if (ahas && !bhas)
    return (SORTCODE(-1));
  if (!ahas && bhas)
    return (SORTCODE(1));

  /* If neither has a label, use aux sort. */
  if (!ahas && !bhas)
  {
    result = perform_auxsort(result, a, b);
    return (SORTCODE(result));
  }

  /* If both have a label, we just do a lexical compare. */
  result = mutt_str_strcasecmp((*ppa)->env->x_label, (*ppb)->env->x_label);
  return (SORTCODE(result));
}

sort_t *mutt_get_sort_func(int method)
{
  switch (method & SORT_MASK)
  {
    case SORT_RECEIVED:
      return compare_date_received;
    case SORT_ORDER:
      return compare_order;
    case SORT_DATE:
      return compare_date_sent;
    case SORT_SUBJECT:
      return compare_subject;
    case SORT_FROM:
      return compare_from;
    case SORT_SIZE:
      return compare_size;
    case SORT_TO:
      return compare_to;
    case SORT_SCORE:
      return compare_score;
    case SORT_SPAM:
      return compare_spam;
    case SORT_LABEL:
      return compare_label;
    default:
      return NULL;
  }
  /* not reached */
}

void mutt_sort_headers(struct Context *ctx, int init)
{
  struct Header *h = NULL;
  struct MuttThread *thread = NULL, *top = NULL;
  sort_t *sortfunc = NULL;

  OPT_NEED_RESORT = false;

  if (!ctx)
    return;

  if (!ctx->msgcount)
  {
    /* this function gets called by mutt_sync_mailbox(), which may have just
     * deleted all the messages.  the virtual message numbers are not updated
     * in that routine, so we must make sure to zero the vcount member.
     */
    ctx->vcount = 0;
    mutt_clear_threads(ctx);
    return; /* nothing to do! */
  }

  if (!ctx->quiet)
    mutt_message(_("Sorting mailbox..."));

  if (OPT_NEED_RESCORE && Score)
  {
    for (int i = 0; i < ctx->msgcount; i++)
      mutt_score_message(ctx, ctx->hdrs[i], 1);
  }
  OPT_NEED_RESCORE = false;

  if (OPT_RESORT_INIT)
  {
    OPT_RESORT_INIT = false;
    init = 1;
  }

  if (init && ctx->tree)
    mutt_clear_threads(ctx);

  if ((Sort & SORT_MASK) == SORT_THREADS)
  {
    AuxSort = NULL;
    /* if $sort_aux changed after the mailbox is sorted, then all the
       subthreads need to be resorted */
    if (OPT_SORT_SUBTHREADS)
    {
      int i = Sort;
      Sort = SortAux;
      if (ctx->tree)
        ctx->tree = mutt_sort_subthreads(ctx->tree, 1);
      Sort = i;
      OPT_SORT_SUBTHREADS = false;
    }
    mutt_sort_threads(ctx, init);
  }
  else if ((sortfunc = mutt_get_sort_func(Sort)) == NULL ||
           (AuxSort = mutt_get_sort_func(SortAux)) == NULL)
  {
    mutt_error(_("Could not find sorting function! [report this bug]"));
    return;
  }
  else
    qsort((void *) ctx->hdrs, ctx->msgcount, sizeof(struct Header *), sortfunc);

  /* adjust the virtual message numbers */
  ctx->vcount = 0;
  for (int i = 0; i < ctx->msgcount; i++)
  {
    struct Header *cur = ctx->hdrs[i];
    if (cur->virtual != -1 || (cur->collapsed && (!ctx->pattern || cur->limited)))
    {
      cur->virtual = ctx->vcount;
      ctx->v2r[ctx->vcount] = i;
      ctx->vcount++;
    }
    cur->msgno = i;
  }

  /* re-collapse threads marked as collapsed */
  if ((Sort & SORT_MASK) == SORT_THREADS)
  {
    top = ctx->tree;
    while ((thread = top) != NULL)
    {
      while (!thread->message)
        thread = thread->child;
      h = thread->message;

      if (h->collapsed)
        mutt_collapse_thread(ctx, h);
      top = top->next;
    }
    mutt_set_virtual(ctx);
  }

  if (!ctx->quiet)
    mutt_clear_error();
}
