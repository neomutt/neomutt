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
#include <string.h>
#include <stdlib.h>

typedef struct score_t
{
  char *str;
  pattern_t *pat;
  int val;
  int exact;		/* if this rule matches, don't evaluate any more */
  struct score_t *next;
} SCORE;

SCORE *Score = NULL;

void mutt_check_rescore (CONTEXT *ctx)
{
  int i;

  if (option (OPTNEEDRESCORE))
  {
    if ((Sort & SORT_MASK) == SORT_SCORE ||
	(SortAux & SORT_MASK) == SORT_SCORE)
    {
      set_option (OPTNEEDRESORT);
      if ((Sort & SORT_MASK) == SORT_THREADS)
	set_option (OPTSORTSUBTHREADS);
    }

    /* must redraw the index since the user might have %N in it */
    set_option (OPTFORCEREDRAWINDEX);
    set_option (OPTFORCEREDRAWPAGER);

    for (i = 0; i < ctx->msgcount; i++)
      mutt_score_message (ctx->hdrs[i]);

    /* force re-caching of index colors */
    for (i = 0; ctx && i < ctx->msgcount; i++)
      ctx->hdrs[i]->pair = 0;

    unset_option (OPTNEEDRESCORE);
  }
}

int mutt_parse_score (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  SCORE *ptr, *last;
  char *pattern, *pc;
  struct pattern_t *pat;

  mutt_extract_token (buf, s, 0);
  if (!MoreArgs (s))
  {
    strfcpy (err->data, _("score: too few arguments"), err->dsize);
    return (-1);
  }
  pattern = buf->data;
  memset (buf, 0, sizeof (BUFFER));
  mutt_extract_token (buf, s, 0);
  if (MoreArgs (s))
  {
    FREE (&pattern);
    strfcpy (err->data, _("score: too many arguments"), err->dsize);
    return (-1);
  }

  /* look for an existing entry and update the value, else add it to the end
     of the list */
  for (ptr = Score, last = NULL; ptr; last = ptr, ptr = ptr->next)
    if (mutt_strcmp (pattern, ptr->str) == 0)
      break;
  if (!ptr)
  {
    if ((pat = mutt_pattern_comp (pattern, 0, err)) == NULL)
    {
      FREE (&pattern);
      return (-1);
    }
    ptr = safe_calloc (1, sizeof (SCORE));
    if (last)
      last->next = ptr;
    else
      Score = ptr;
    ptr->pat = pat;
    ptr->str = pattern;
  }
  pc = buf->data;
  if (*pc == '=')
  {
    ptr->exact = 1;
    pc++;
  }
  ptr->val = atoi (pc);
  set_option (OPTNEEDRESCORE);
  return 0;
}

void mutt_score_message (HEADER *hdr)
{
  SCORE *tmp;

  hdr->score = 0; /* in case of re-scoring */
  for (tmp = Score; tmp; tmp = tmp->next)
  {
    if (mutt_pattern_exec (tmp->pat, 0, NULL, hdr) > 0)
    {
      if (tmp->exact || tmp->val == 9999 || tmp->val == -9999)
      {
	hdr->score = tmp->val;
	break;
      }
      hdr->score += tmp->val;
    }
  }
  if (hdr->score < 0)
    hdr->score = 0;
}

int mutt_parse_unscore (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  SCORE *tmp, *last = NULL;

  while (MoreArgs (s))
  {
    mutt_extract_token (buf, s, 0);
    if (!mutt_strcmp ("*", buf->data))
    {
      for (tmp = Score; tmp; )
      {
	last = tmp;
	tmp = tmp->next;
	mutt_pattern_free (&last->pat);
	safe_free ((void **) &last);
      }
      Score = NULL;
    }
    else
    {
      for (tmp = Score; tmp; last = tmp, tmp = tmp->next)
      {
	if (!mutt_strcmp (buf->data, tmp->str))
	{
	  if (last)
	    last->next = tmp->next;
	  else
	    Score = tmp->next;
	  mutt_pattern_free (&tmp->pat);
	  safe_free ((void **) &tmp);
	  /* there should only be one score per pattern, so we can stop here */
	  break;
	}
      }
    }
  }
  set_option (OPTNEEDRESCORE);
  return 0;
}
