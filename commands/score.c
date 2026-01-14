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
 * @page commands_score Routines for adding user scores to emails
 *
 * Routines for adding user scores to emails
 */

#include "config.h"
#include <stdbool.h>
#include <stdlib.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "score.h"
#include "index/lib.h"
#include "parse/lib.h"
#include "pattern/lib.h"
#include "globals.h"

/**
 * parse_score - Parse the 'score' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `score <pattern> <value>`
 */
enum CommandResult parse_score(const struct Command *cmd, struct Buffer *line,
                               struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Score *ptr = NULL, *last = NULL;
  char *pattern = NULL, *pc = NULL;
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  parse_extract_token(token, line, TOKEN_NO_FLAGS);
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    goto done;
  }
  pattern = buf_strdup(token);
  parse_extract_token(token, line, TOKEN_NO_FLAGS);
  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    goto done;
  }

  /* look for an existing entry and update the value, else add it to the end
   * of the list */
  for (ptr = ScoreList, last = NULL; ptr; last = ptr, ptr = ptr->next)
    if (mutt_str_equal(pattern, ptr->str))
      break;

  if (ptr)
  {
    /* 'token' arg was cleared and 'pattern' holds the only reference;
     * as here 'ptr' != NULL -> update the value only in which case
     * ptr->str already has the string, so pattern should be freed.  */
    FREE(&pattern);
  }
  else
  {
    struct MailboxView *mv_cur = get_current_mailbox_view();
    struct PatternList *pat = mutt_pattern_comp(mv_cur, pattern, MUTT_PC_NO_FLAGS, err);
    if (!pat)
    {
      goto done;
    }
    ptr = MUTT_MEM_CALLOC(1, struct Score);
    if (last)
      last->next = ptr;
    else
      ScoreList = ptr;
    ptr->pat = pat;
    ptr->str = pattern;
    pattern = NULL;
  }

  pc = token->data;
  if (*pc == '=')
  {
    ptr->exact = true;
    pc++;
  }

  if (!mutt_str_atoi_full(pc, &ptr->val))
  {
    buf_strcpy(err, _("Error: score: invalid number"));
    goto done;
  }
  OptNeedRescore = true;

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  FREE(&pattern);
  return rc;
}

/**
 * parse_unscore - Parse the 'unscore' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unscore { * | <pattern> ... }`
 */
enum CommandResult parse_unscore(const struct Command *cmd, struct Buffer *line,
                                 struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Score *tmp = NULL, *last = NULL;
  struct Buffer *token = buf_pool_get();

  while (MoreArgs(line))
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    if (mutt_str_equal("*", buf_string(token)))
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
        if (mutt_str_equal(buf_string(token), tmp->str))
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
  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}
