/**
 * @file
 * Routines for adding user scores to emails
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2017-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018 Victor Fernandes <criw@pm.me>
 * Copyright (C) 2020-2021 Pietro Cerutti <gahr@gahr.ch>
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
 * @page email_score Routines for adding user scores to emails
 *
 * Routines for adding user scores to emails
 */

#include "config.h"
#include <stdbool.h>
#include <stdlib.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "score.h"
#include "pattern/lib.h"
#include "email.h"
#include "globals.h"
#include "mutt_thread.h"
#include "sort.h"

/// Linked list of email scoring rules
struct Score *ScoreList = NULL;

/**
 * mutt_check_rescore - Do the emails need to have their scores recalculated?
 * @param m Mailbox
 */
void mutt_check_rescore(struct Mailbox *m)
{
  const bool c_score = cs_subset_bool(NeoMutt->sub, "score");
  if (OptNeedRescore && c_score)
  {
    const enum EmailSortType c_sort = cs_subset_sort(NeoMutt->sub, "sort");
    const enum EmailSortType c_sort_aux = cs_subset_sort(NeoMutt->sub, "sort_aux");
    if (((c_sort & SORT_MASK) == EMAIL_SORT_SCORE) ||
        ((c_sort_aux & SORT_MASK) == EMAIL_SORT_SCORE))
    {
      OptNeedResort = true;
      if (mutt_using_threads())
        OptSortSubthreads = true;
    }

    mutt_debug(LL_NOTIFY, "NT_SCORE: %p\n", (void *) m);
    notify_send(m->notify, NT_SCORE, 0, NULL);
  }
  OptNeedRescore = false;
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

  const short c_score_threshold_delete = cs_subset_number(NeoMutt->sub, "score_threshold_delete");
  const short c_score_threshold_flag = cs_subset_number(NeoMutt->sub, "score_threshold_flag");
  const short c_score_threshold_read = cs_subset_number(NeoMutt->sub, "score_threshold_read");

  if (e->score <= c_score_threshold_delete)
    mutt_set_flag(m, e, MUTT_DELETE, true, upd_mbox);
  if (e->score <= c_score_threshold_read)
    mutt_set_flag(m, e, MUTT_READ, true, upd_mbox);
  if (e->score >= c_score_threshold_flag)
    mutt_set_flag(m, e, MUTT_FLAG, true, upd_mbox);
}
