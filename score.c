/**
 * @file
 * Routines for adding user scores to emails
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
#include "mutt.h"
#include "context.h"
#include "globals.h"
#include "header.h"
#include "keymap.h"
#include "mutt_menu.h"
#include "options.h"
#include "pattern.h"
#include "protos.h"
#include "sort.h"

/**
 * struct Score - Scoring rule for email
 */
struct Score
{
  char *str;
  struct Pattern *pat;
  int val;
  int exact; /**< if this rule matches, don't evaluate any more */
  struct Score *next;
};

static struct Score *ScoreList = NULL;

void mutt_check_rescore(struct Context *ctx)
{
  if (OptNeedRescore && Score)
  {
    if ((Sort & SORT_MASK) == SORT_SCORE || (SortAux & SORT_MASK) == SORT_SCORE)
    {
      OptNeedResort = true;
      if ((Sort & SORT_MASK) == SORT_THREADS)
        OptSortSubthreads = true;
    }

    /* must redraw the index since the user might have %N in it */
    mutt_menu_set_redraw_full(MENU_MAIN);
    mutt_menu_set_redraw_full(MENU_PAGER);

    for (int i = 0; ctx && i < ctx->msgcount; i++)
    {
      mutt_score_message(ctx, ctx->hdrs[i], 1);
      ctx->hdrs[i]->pair = 0;
    }
  }
  OptNeedRescore = false;
}

int mutt_parse_score(struct Buffer *buf, struct Buffer *s, unsigned long data,
                     struct Buffer *err)
{
  struct Score *ptr = NULL, *last = NULL;
  char *pattern = NULL, *pc = NULL;
  struct Pattern *pat = NULL;

  mutt_extract_token(buf, s, 0);
  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "score");
    return -1;
  }
  pattern = buf->data;
  mutt_buffer_init(buf);
  mutt_extract_token(buf, s, 0);
  if (MoreArgs(s))
  {
    FREE(&pattern);
    mutt_buffer_printf(err, _("%s: too many arguments"), "score");
    return -1;
  }

  /* look for an existing entry and update the value, else add it to the end
     of the list */
  for (ptr = ScoreList, last = NULL; ptr; last = ptr, ptr = ptr->next)
    if (mutt_str_strcmp(pattern, ptr->str) == 0)
      break;
  if (!ptr)
  {
    pat = mutt_pattern_comp(pattern, 0, err);
    if (!pat)
    {
      FREE(&pattern);
      return -1;
    }
    ptr = mutt_mem_calloc(1, sizeof(struct Score));
    if (last)
      last->next = ptr;
    else
      ScoreList = ptr;
    ptr->pat = pat;
    ptr->str = pattern;
  }
  else
    /* 'buf' arg was cleared and 'pattern' holds the only reference;
     * as here 'ptr' != NULL -> update the value only in which case
     * ptr->str already has the string, so pattern should be freed.
     */
    FREE(&pattern);
  pc = buf->data;
  if (*pc == '=')
  {
    ptr->exact = 1;
    pc++;
  }
  if (mutt_str_atoi(pc, &ptr->val) < 0)
  {
    FREE(&pattern);
    mutt_str_strfcpy(err->data, _("Error: score: invalid number"), err->dsize);
    return -1;
  }
  OptNeedRescore = true;
  return 0;
}

void mutt_score_message(struct Context *ctx, struct Header *hdr, int upd_ctx)
{
  struct Score *tmp = NULL;
  struct PatternCache cache;

  memset(&cache, 0, sizeof(cache));
  hdr->score = 0; /* in case of re-scoring */
  for (tmp = ScoreList; tmp; tmp = tmp->next)
  {
    if (mutt_pattern_exec(tmp->pat, MUTT_MATCH_FULL_ADDRESS, NULL, hdr, &cache) > 0)
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

  if (hdr->score <= ScoreThresholdDelete)
    mutt_set_flag_update(ctx, hdr, MUTT_DELETE, 1, upd_ctx);
  if (hdr->score <= ScoreThresholdRead)
    mutt_set_flag_update(ctx, hdr, MUTT_READ, 1, upd_ctx);
  if (hdr->score >= ScoreThresholdFlag)
    mutt_set_flag_update(ctx, hdr, MUTT_FLAG, 1, upd_ctx);
}

int mutt_parse_unscore(struct Buffer *buf, struct Buffer *s, unsigned long data,
                       struct Buffer *err)
{
  struct Score *tmp = NULL, *last = NULL;

  while (MoreArgs(s))
  {
    mutt_extract_token(buf, s, 0);
    if (mutt_str_strcmp("*", buf->data) == 0)
    {
      for (tmp = ScoreList; tmp;)
      {
        last = tmp;
        tmp = tmp->next;
        mutt_pattern_free(&last->pat);
        FREE(&last);
      }
      ScoreList = NULL;
    }
    else
    {
      for (tmp = ScoreList; tmp; last = tmp, tmp = tmp->next)
      {
        if (mutt_str_strcmp(buf->data, tmp->str) == 0)
        {
          if (last)
            last->next = tmp->next;
          else
            ScoreList = tmp->next;
          mutt_pattern_free(&tmp->pat);
          FREE(&tmp);
          /* there should only be one score per pattern, so we can stop here */
          break;
        }
      }
    }
  }
  OptNeedRescore = true;
  return 0;
}
