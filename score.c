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

/**
 * @page score Routines for adding user scores to emails
 *
 * Routines for adding user scores to emails
 */

#include "config.h"
#include <stdbool.h>
#include <stdlib.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "score.h"
#include "init.h"
#include "keymap.h"
#include "mutt_commands.h"
#include "mutt_globals.h"
#include "mutt_menu.h"
#include "options.h"
#include "pattern.h"
#include "protos.h"
#include "sort.h"

/* These Config Variables are only used in score.c */
short C_ScoreThresholdDelete; ///< Config: Messages with a lower score will be automatically deleted
short C_ScoreThresholdFlag; ///< Config: Messages with a greater score will be automatically flagged
short C_ScoreThresholdRead; ///< Config: Messages with a lower score will be automatically marked read

/**
 * struct Score - Scoring rule for email
 */
struct Score
{
  char *str;
  struct PatternList *pat;
  int val;
  bool exact; ///< if this rule matches, don't evaluate any more
  struct Score *next;
};

static struct Score *ScoreList = NULL;

/**
 * mutt_check_rescore - Do the emails need to have their scores recalculated?
 * @param m Mailbox
 */
void mutt_check_rescore(struct Mailbox *m)
{
  if (OptNeedRescore && C_Score)
  {
    if (((C_Sort & SORT_MASK) == SORT_SCORE) || ((C_SortAux & SORT_MASK) == SORT_SCORE))
    {
      OptNeedResort = true;
      if ((C_Sort & SORT_MASK) == SORT_THREADS)
        OptSortSubthreads = true;
    }

    /* must redraw the index since the user might have %N in it */
    mutt_menu_set_redraw_full(MENU_MAIN);
    mutt_menu_set_redraw_full(MENU_PAGER);

    for (int i = 0; m && i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;

      mutt_score_message(m, e, true);
      e->pair = 0;
    }
  }
  OptNeedRescore = false;
}

/**
 * mutt_parse_score - Parse the 'score' command - Implements Command::parse()
 */
enum CommandResult mutt_parse_score(struct Buffer *buf, struct Buffer *s,
                                    intptr_t data, struct Buffer *err)
{
  struct Score *ptr = NULL, *last = NULL;
  char *pattern = NULL, *pc = NULL;

  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "score");
    return MUTT_CMD_WARNING;
  }
  pattern = mutt_buffer_strdup(buf);
  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
  if (MoreArgs(s))
  {
    FREE(&pattern);
    mutt_buffer_printf(err, _("%s: too many arguments"), "score");
    return MUTT_CMD_WARNING;
  }

  /* look for an existing entry and update the value, else add it to the end
   * of the list */
  for (ptr = ScoreList, last = NULL; ptr; last = ptr, ptr = ptr->next)
    if (mutt_str_equal(pattern, ptr->str))
      break;
  if (!ptr)
  {
    struct PatternList *pat = mutt_pattern_comp(pattern, MUTT_PC_NO_FLAGS, err);
    if (!pat)
    {
      FREE(&pattern);
      return MUTT_CMD_ERROR;
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
  {
    /* 'buf' arg was cleared and 'pattern' holds the only reference;
     * as here 'ptr' != NULL -> update the value only in which case
     * ptr->str already has the string, so pattern should be freed.  */
    FREE(&pattern);
  }
  pc = buf->data;
  if (*pc == '=')
  {
    ptr->exact = true;
    pc++;
  }
  if (mutt_str_atoi(pc, &ptr->val) < 0)
  {
    FREE(&pattern);
    mutt_buffer_strcpy(err, _("Error: score: invalid number"));
    return MUTT_CMD_ERROR;
  }
  OptNeedRescore = true;
  return MUTT_CMD_SUCCESS;
}

/**
 * mutt_score_message - Apply scoring to an email
 * @param m        Mailbox
 * @param e        Email
 * @param upd_mbox If true, update the Mailbox too
 */
void mutt_score_message(struct Mailbox *m, struct Email *e, bool upd_mbox)
{
  struct Score *tmp = NULL;
  struct PatternCache cache = { 0 };

  e->score = 0; /* in case of re-scoring */
  for (tmp = ScoreList; tmp; tmp = tmp->next)
  {
    if (mutt_pattern_exec(SLIST_FIRST(tmp->pat), MUTT_MATCH_FULL_ADDRESS, NULL, e, &cache) > 0)
    {
      if (tmp->exact || (tmp->val == 9999) || (tmp->val == -9999))
      {
        e->score = tmp->val;
        break;
      }
      e->score += tmp->val;
    }
  }
  if (e->score < 0)
    e->score = 0;

  if (e->score <= C_ScoreThresholdDelete)
    mutt_set_flag_update(m, e, MUTT_DELETE, true, upd_mbox);
  if (e->score <= C_ScoreThresholdRead)
    mutt_set_flag_update(m, e, MUTT_READ, true, upd_mbox);
  if (e->score >= C_ScoreThresholdFlag)
    mutt_set_flag_update(m, e, MUTT_FLAG, true, upd_mbox);
}

/**
 * mutt_parse_unscore - Parse the 'unscore' command - Implements Command::parse()
 */
enum CommandResult mutt_parse_unscore(struct Buffer *buf, struct Buffer *s,
                                      intptr_t data, struct Buffer *err)
{
  struct Score *tmp = NULL, *last = NULL;

  while (MoreArgs(s))
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    if (mutt_str_equal("*", buf->data))
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
        if (mutt_str_equal(buf->data, tmp->str))
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
  return MUTT_CMD_SUCCESS;
}
