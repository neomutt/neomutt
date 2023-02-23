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
 * @page neo_score Routines for adding user scores to emails
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
#include "index/lib.h"
#include "pattern/lib.h"
#include "init.h"
#include "mutt_thread.h"
#include "options.h"
#include "protos.h"

#include <string>
#include <list>

/**
 * struct Score - Scoring rule for email
 */
struct Score
{
  Score(std::string str, struct PatternList *pat)
    : str{ std::move(str) }
    , pat{ pat }
  {}

  std::string str;
  struct PatternList *pat = nullptr;
  int val = 0;
  bool exact = false; ///< if this rule matches, don't evaluate any more
};

static std::list<Score> ScoreList;

/**
 * mutt_check_rescore - Do the emails need to have their scores recalculated?
 * @param m Mailbox
 */
void mutt_check_rescore(struct Mailbox *m)
{
  const bool c_score = cs_subset_bool(NeoMutt->sub, "score");
  if (OptNeedRescore && c_score)
  {
    const short c_sort = cs_subset_sort(NeoMutt->sub, "sort");
    const short c_sort_aux = cs_subset_sort(NeoMutt->sub, "sort_aux");
    if (((c_sort & SORT_MASK) == SORT_SCORE) || ((c_sort_aux & SORT_MASK) == SORT_SCORE))
    {
      OptNeedResort = true;
      if (mutt_using_threads())
        OptSortSubthreads = true;
    }

    mutt_debug(LL_NOTIFY, "NT_SCORE: %p\n", m);
    notify_send(m->notify, NT_SCORE, 0, NULL);
  }
  OptNeedRescore = false;
}

/**
 * mutt_parse_score - Parse the 'score' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_parse_score(struct Buffer *buf, struct Buffer *s,
                                    intptr_t data, struct Buffer *err)
{
  struct Score *ptr = NULL;
  char *pc = NULL;

  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "score");
    return MUTT_CMD_WARNING;
  }
  std::string pattern{ mutt_buffer_string(buf) };
  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
  if (MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too many arguments"), "score");
    return MUTT_CMD_WARNING;
  }

  /* look for an existing entry and update the value, else add it to the end
   * of the list */
  const auto matchStr = [&pattern](const Score& score)
  {
    return score.str == pattern;
  };
  if (const auto it{ std::find_if(begin(ScoreList), end(ScoreList), matchStr) };
      it == end(ScoreList))
  {
    struct Mailbox *m_cur = get_current_mailbox();
    struct Menu *menu = get_current_menu();
    struct PatternList *pat = mutt_pattern_comp(m_cur, menu, pattern.c_str(), MUTT_PC_NO_FLAGS, err);
    if (!pat)
    {
      return MUTT_CMD_ERROR;
    }
    ScoreList.emplace_back(pattern, pat);
  }
  pc = buf->data;
  if (*pc == '=')
  {
    ptr->exact = true;
    pc++;
  }
  if (!mutt_str_atoi_full(pc, &ptr->val))
  {
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
  struct PatternCache cache = { 0 };

  e->score = 0; /* in case of re-scoring */
  for (const auto& score : ScoreList)
  {
    if (mutt_pattern_exec(SLIST_FIRST(score.pat), MUTT_MATCH_FULL_ADDRESS, NULL, e, &cache) > 0)
    {
      if (score.exact || (score.val == 9999) || (score.val == -9999))
      {
        e->score = score.val;
        break;
      }
      e->score += score.val;
    }
  }
  if (e->score < 0)
    e->score = 0;

  const short c_score_threshold_delete = cs_subset_number(NeoMutt->sub, "score_threshold_delete");
  const short c_score_threshold_flag = cs_subset_number(NeoMutt->sub, "score_threshold_flag");
  const short c_score_threshold_read = cs_subset_number(NeoMutt->sub, "score_threshold_read");

  if (e->score <= c_score_threshold_delete)
    mutt_set_flag_update(m, e, MUTT_DELETE, true, upd_mbox);
  if (e->score <= c_score_threshold_read)
    mutt_set_flag_update(m, e, MUTT_READ, true, upd_mbox);
  if (e->score >= c_score_threshold_flag)
    mutt_set_flag_update(m, e, MUTT_FLAG, true, upd_mbox);
}

/**
 * mutt_parse_unscore - Parse the 'unscore' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_parse_unscore(struct Buffer *buf, struct Buffer *s,
                                      intptr_t data, struct Buffer *err)
{
  std::string key{ mutt_buffer_string(buf) };

  while (MoreArgs(s))
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    if (mutt_str_equal("*", buf->data))
    {
      for (auto& score: ScoreList)
      {
        mutt_pattern_free(&score.pat);
      }
      ScoreList.clear();
    }
    else
    {
      const auto matchStr = [&key](const Score& score)
      {
        return score.str == key;
      };
      if (const auto it{ std::find_if(begin(ScoreList), end(ScoreList), matchStr) };
        it != end(ScoreList))
      {
        mutt_pattern_free(&it->pat);
        ScoreList.erase(it);
      }
      /* there should only be one score per pattern, so we can stop here */
    }
  }
  OptNeedRescore = true;
  return MUTT_CMD_SUCCESS;
}
