/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "sort.h"
#include "mutt_idna.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define SORTCODE(x) (Sort & SORT_REVERSE) ? -(x) : x

/* function to use as discriminator when normal sort method is equal */
static sort_t *AuxSort = NULL;

#define AUXSORT(code,a,b) if (!code && AuxSort && !option(OPTAUXSORT)) { \
  set_option(OPTAUXSORT); \
  code = AuxSort(a,b); \
  unset_option(OPTAUXSORT); \
} \
if (!code) \
  code = (*((HEADER **)a))->index - (*((HEADER **)b))->index;

static int compare_score (const void *a, const void *b)
{
  HEADER **pa = (HEADER **) a;
  HEADER **pb = (HEADER **) b;
  int result = (*pb)->score - (*pa)->score; /* note that this is reverse */
  AUXSORT(result,a,b);
  return (SORTCODE (result));
}

static int compare_size (const void *a, const void *b)
{
  HEADER **pa = (HEADER **) a;
  HEADER **pb = (HEADER **) b;
  int result = (*pa)->content->length - (*pb)->content->length;
  AUXSORT(result,a,b);
  return (SORTCODE (result));
}

static int compare_date_sent (const void *a, const void *b)
{
  HEADER **pa = (HEADER **) a;
  HEADER **pb = (HEADER **) b;
  int result = (*pa)->date_sent - (*pb)->date_sent;
  AUXSORT(result,a,b);
  return (SORTCODE (result));
}

static int compare_subject (const void *a, const void *b)
{
  HEADER **pa = (HEADER **) a;
  HEADER **pb = (HEADER **) b;
  int rc;

  if (!(*pa)->env->real_subj)
  {
    if (!(*pb)->env->real_subj)
      rc = compare_date_sent (pa, pb);
    else
      rc = -1;
  }
  else if (!(*pb)->env->real_subj)
    rc = 1;
  else
    rc = mutt_strcasecmp ((*pa)->env->real_subj, (*pb)->env->real_subj);
  AUXSORT(rc,a,b);
  return (SORTCODE (rc));
}

const char *mutt_get_name (ADDRESS *a)
{
  ADDRESS *ali;

  if (a)
  {
    if (option (OPTREVALIAS) && (ali = alias_reverse_lookup (a)) && ali->personal)
      return ali->personal;
    else if (a->personal)
      return a->personal;
    else if (a->mailbox)
      return (mutt_addr_for_display (a));
  }
  /* don't return NULL to avoid segfault when printing/comparing */
  return ("");
}

static int compare_to (const void *a, const void *b)
{
  HEADER **ppa = (HEADER **) a;
  HEADER **ppb = (HEADER **) b;
  char fa[SHORT_STRING];
  const char *fb;
  int result;

  strfcpy (fa, mutt_get_name ((*ppa)->env->to), SHORT_STRING);
  fb = mutt_get_name ((*ppb)->env->to);
  result = mutt_strncasecmp (fa, fb, SHORT_STRING);
  AUXSORT(result,a,b);
  return (SORTCODE (result));
}

static int compare_from (const void *a, const void *b)
{
  HEADER **ppa = (HEADER **) a;
  HEADER **ppb = (HEADER **) b;
  char fa[SHORT_STRING];
  const char *fb;
  int result;

  strfcpy (fa, mutt_get_name ((*ppa)->env->from), SHORT_STRING);
  fb = mutt_get_name ((*ppb)->env->from);
  result = mutt_strncasecmp (fa, fb, SHORT_STRING);
  AUXSORT(result,a,b);
  return (SORTCODE (result));
}

static int compare_date_received (const void *a, const void *b)
{
  HEADER **pa = (HEADER **) a;
  HEADER **pb = (HEADER **) b;
  int result = (*pa)->received - (*pb)->received;
  AUXSORT(result,a,b);
  return (SORTCODE (result));
}

static int compare_order (const void *a, const void *b)
{
  HEADER **ha = (HEADER **) a;
  HEADER **hb = (HEADER **) b;

  /* no need to auxsort because you will never have equality here */
  return (SORTCODE ((*ha)->index - (*hb)->index));
}

static int compare_spam (const void *a, const void *b)
{
  HEADER **ppa = (HEADER **) a;
  HEADER **ppb = (HEADER **) b;
  char   *aptr, *bptr;
  int     ahas, bhas;
  int     result = 0;
  double  difference;

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
    AUXSORT(result, a, b);
    return (SORTCODE(result));
  }


  /* Both have spam attrs. */

  /* preliminary numeric examination */
  difference = (strtod((*ppa)->env->spam->data, &aptr) -
                strtod((*ppb)->env->spam->data, &bptr));

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
      AUXSORT(result, a, b);
  }

  return (SORTCODE(result));
}

sort_t *mutt_get_sort_func (int method)
{
  switch (method & SORT_MASK)
  {
    case SORT_RECEIVED:
      return (compare_date_received);
    case SORT_ORDER:
      return (compare_order);
    case SORT_DATE:
      return (compare_date_sent);
    case SORT_SUBJECT:
      return (compare_subject);
    case SORT_FROM:
      return (compare_from);
    case SORT_SIZE:
      return (compare_size);
    case SORT_TO:
      return (compare_to);
    case SORT_SCORE:
      return (compare_score);
    case SORT_SPAM:
      return (compare_spam);
    default:
      return (NULL);
  }
  /* not reached */
}

void mutt_sort_headers (CONTEXT *ctx, int init)
{
  int i;
  HEADER *h;
  THREAD *thread, *top;
  sort_t *sortfunc;
  
  unset_option (OPTNEEDRESORT);

  if (!ctx)
    return;

  if (!ctx->msgcount)
  {
    /* this function gets called by mutt_sync_mailbox(), which may have just
     * deleted all the messages.  the virtual message numbers are not updated
     * in that routine, so we must make sure to zero the vcount member.
     */
    ctx->vcount = 0;
    mutt_clear_threads (ctx);
    return; /* nothing to do! */
  }

  if (!ctx->quiet)
    mutt_message _("Sorting mailbox...");

  if (option (OPTNEEDRESCORE) && option (OPTSCORE))
  {
    for (i = 0; i < ctx->msgcount; i++)
      mutt_score_message (ctx, ctx->hdrs[i], 1);
  }
  unset_option (OPTNEEDRESCORE);

  if (option (OPTRESORTINIT))
  {
    unset_option (OPTRESORTINIT);
    init = 1;
  }

  if (init && ctx->tree)
    mutt_clear_threads (ctx);

  if ((Sort & SORT_MASK) == SORT_THREADS)
  {
    AuxSort = NULL;
    /* if $sort_aux changed after the mailbox is sorted, then all the
       subthreads need to be resorted */
    if (option (OPTSORTSUBTHREADS))
    {
      i = Sort;
      Sort = SortAux;
      if (ctx->tree)
	ctx->tree = mutt_sort_subthreads (ctx->tree, 1);
      Sort = i;
      unset_option (OPTSORTSUBTHREADS);
    }
    mutt_sort_threads (ctx, init);
  }
  else if ((sortfunc = mutt_get_sort_func (Sort)) == NULL ||
	   (AuxSort = mutt_get_sort_func (SortAux)) == NULL)
  {
    mutt_error _("Could not find sorting function! [report this bug]");
    mutt_sleep (1);
    return;
  }
  else 
    qsort ((void *) ctx->hdrs, ctx->msgcount, sizeof (HEADER *), sortfunc);

  /* adjust the virtual message numbers */
  ctx->vcount = 0;
  for (i = 0; i < ctx->msgcount; i++)
  {
    HEADER *cur = ctx->hdrs[i];
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
	mutt_collapse_thread (ctx, h);
      top = top->next;
    }
    mutt_set_virtual (ctx);
  }

  if (!ctx->quiet)
    mutt_clear_error ();
}
