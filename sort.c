static const char rcsid[]="$Id$";
/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#include "mutt.h"
#include "sort.h"

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
}

int compare_score (const void *a, const void *b)
{
  HEADER **pa = (HEADER **) a;
  HEADER **pb = (HEADER **) b;
  int result = (*pb)->score - (*pa)->score; /* note that this is reverse */
  AUXSORT(result,a,b);
  return (SORTCODE (result));
}

int compare_size (const void *a, const void *b)
{
  HEADER **pa = (HEADER **) a;
  HEADER **pb = (HEADER **) b;
  int result = (*pa)->content->length - (*pb)->content->length;
  AUXSORT(result,a,b);
  return (SORTCODE (result));
}

int compare_date_sent (const void *a, const void *b)
{
  HEADER **pa = (HEADER **) a;
  HEADER **pb = (HEADER **) b;
  int result = (*pa)->date_sent - (*pb)->date_sent;
  AUXSORT(result,a,b);
  return (SORTCODE (result));
}

int compare_subject (const void *a, const void *b)
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
    rc = strcasecmp ((*pa)->env->real_subj, (*pb)->env->real_subj);
  AUXSORT(rc,a,b);
  return (SORTCODE (rc));
}

char *mutt_get_name (ADDRESS *a)
{
  ADDRESS *ali;

  if (a)
  {
    if (option (OPTREVALIAS) && (ali = alias_reverse_lookup (a)) && ali->personal)
      return ali->personal;
    else if (a->personal)
      return a->personal;
    else if (a->mailbox)
      return (a->mailbox);
  }
  /* don't return NULL to avoid segfault when printing/comparing */
  return ("");
}

int compare_to (const void *a, const void *b)
{
  HEADER **ppa = (HEADER **) a;
  HEADER **ppb = (HEADER **) b;
  char *fa, *fb;
  int result;

  fa = mutt_get_name ((*ppa)->env->to);
  fb = mutt_get_name ((*ppb)->env->to);
  result = strcasecmp (fa, fb);
  AUXSORT(result,a,b);
  return (SORTCODE (result));
}

int compare_from (const void *a, const void *b)
{
  HEADER **ppa = (HEADER **) a;
  HEADER **ppb = (HEADER **) b;
  char *fa, *fb;
  int result;

  fa = mutt_get_name ((*ppa)->env->from);
  fb = mutt_get_name ((*ppb)->env->from);
  result = strcasecmp (fa, fb);
  AUXSORT(result,a,b);
  return (SORTCODE (result));
}

int compare_date_received (const void *a, const void *b)
{
  HEADER **pa = (HEADER **) a;
  HEADER **pb = (HEADER **) b;
  int result = (*pa)->received - (*pb)->received;
  AUXSORT(result,a,b);
  return (SORTCODE (result));
}

int compare_order (const void *a, const void *b)
{
  HEADER **ha = (HEADER **) a;
  HEADER **hb = (HEADER **) b;

  /* no need to auxsort because you will never have equality here */
  return (SORTCODE ((*ha)->index - (*hb)->index));
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
    default:
      return (NULL);
  }
  /* not reached */
}

void mutt_sort_headers (CONTEXT *ctx, int init)
{
  int i;
  sort_t *sortfunc;
  int collapse = (option (OPTSORTCOLLAPSE)) ? 0 : 1;
  
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
    ctx->tree = 0;
    return; /* nothing to do! */
  }

  if (!ctx->quiet)
    mutt_message _("Sorting mailbox...");

  /* threads may be bogus, so clear the links */
  if (init)
    mutt_clear_threads (ctx);

  if (option (OPTNEEDRESCORE))
  {
    for (i = 0; i < ctx->msgcount; i++)
      mutt_score_message (ctx->hdrs[i]);
    unset_option (OPTNEEDRESCORE);
  }

  if ((Sort & SORT_MASK) == SORT_THREADS)
  {
    AuxSort = NULL;
    /* if $sort_aux changed after the mailbox is sorted, then all the
       subthreads need to be resorted */
    if (option (OPTSORTSUBTHREADS))
    {
      ctx->tree = mutt_sort_subthreads (ctx->tree, mutt_get_sort_func (SortAux));
      unset_option (OPTSORTSUBTHREADS);
    }
    mutt_sort_threads (ctx, init);
  }
  else if ((sortfunc = mutt_get_sort_func (Sort)) == NULL ||
	   (AuxSort = mutt_get_sort_func (SortAux)) == NULL)
  {
    mutt_error _("Could not find sorting function! [report this bug]");
    sleep (1);
    return;
  }
  else 
    qsort ((void *) ctx->hdrs, ctx->msgcount, sizeof (HEADER *), sortfunc);

  /* the threading function find_reference() needs to know how the mailbox
   * is currently sorted in memory in order to speed things up a bit
   */
  ctx->revsort = (Sort & SORT_REVERSE) ? 1 : 0;

  /* adjust the virtual message numbers */
  ctx->vcount = 0;
  if (collapse)
    ctx->collapsed = 0;
  for (i = 0; i < ctx->msgcount; i++)
  {
    HEADER *cur = ctx->hdrs[i];
    if (cur->virtual != -1 || (collapse && cur->collapsed && (!ctx->pattern || cur->limited)))
    {
      cur->virtual = ctx->vcount;
      ctx->v2r[ctx->vcount] = i;
      ctx->vcount++;
    }
    cur->msgno = i;
    cur->num_hidden = mutt_get_hidden (ctx, cur);
    if (collapse)
      cur->collapsed = 0;
  }

  mutt_cache_index_colors(ctx);

  if (!ctx->quiet)
    mutt_clear_error ();
}
