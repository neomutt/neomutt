/**
 * @file
 * Notmuch Auto-Completion
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page nm_complete Notmuch Auto-Completion
 *
 * Notmuch Auto-Completion
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "complete/lib.h"
#include "editor/lib.h"
#include "index/lib.h"
#include "notmuch/lib.h"

/**
 * complete_all_nm_tags - Pass a list of Notmuch tags to the completion code
 * @param cd Completion Data
 * @param pt List of all Notmuch tags
 * @retval  0 Success
 * @retval -1 Error
 */
int complete_all_nm_tags(struct CompletionData *cd, const char *pt)
{
  struct Mailbox *m_cur = get_current_mailbox();
  int tag_count_1 = 0;
  int tag_count_2 = 0;
  int rc = -1;

  mutt_str_copy(cd->user_typed, pt, sizeof(cd->user_typed));
  memset(cd->match_list, 0, cd->match_list_len);
  memset(cd->completed, 0, sizeof(cd->completed));
  cd->free_match_strings = true;

  nm_db_longrun_init(m_cur, false);

  /* Work out how many tags there are. */
  if ((nm_get_all_tags(m_cur, NULL, &tag_count_1) != 0) || (tag_count_1 == 0))
    goto done;

  /* Get all the tags. */
  const char **nm_tags = mutt_mem_calloc(tag_count_1, sizeof(char *));
  if ((nm_get_all_tags(m_cur, nm_tags, &tag_count_2) != 0) || (tag_count_1 != tag_count_2))
  {
    completion_data_free_match_strings(cd);
    goto done;
  }

  /* Put them into the completion machinery. */
  for (int i = 0; i < tag_count_1; i++)
  {
    if (!candidate(cd, cd->user_typed, nm_tags[i], cd->completed, sizeof(cd->completed)))
      FREE(&nm_tags[i]);
  }

  matches_ensure_morespace(cd, cd->num_matched);
  cd->match_list[cd->num_matched++] = mutt_str_dup(cd->user_typed);
  rc = 0;

done:
  FREE(&nm_tags);
  nm_db_longrun_done(m_cur);
  return rc;
}

/**
 * mutt_nm_query_complete - Complete to the nearest notmuch tag
 * @param cd      Completion Data
 * @param buf     Buffer for the result
 * @param pos     Cursor position in the buffer
 * @param numtabs Number of times the user has hit 'tab'
 * @retval true  Success, a match
 * @retval false Error, no match
 *
 * Complete the nearest "tag:"-prefixed string previous to pos.
 */
bool mutt_nm_query_complete(struct CompletionData *cd, struct Buffer *buf, int pos, int numtabs)
{
  char *pt = buf->data;
  int spaces;

  SKIPWS(pt);
  spaces = pt - buf->data;

  pt = (char *) mutt_strn_rfind((char *) buf, pos, "tag:");
  if (pt)
  {
    pt += 4;
    if (numtabs == 1)
    {
      /* First TAB. Collect all the matches */
      complete_all_nm_tags(cd, pt);

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time.  */
      if (cd->user_typed[0] == '\0')
        return true;
    }

    if ((cd->completed[0] == '\0') && (cd->user_typed[0] != '\0'))
      return false;

    /* cd->num_matched will _always_ be at least 1 since the initial
     * user-typed string is always stored */
    if ((numtabs == 1) && (cd->num_matched == 2))
    {
      snprintf(cd->completed, sizeof(cd->completed), "%s", cd->match_list[0]);
    }
    else if ((numtabs > 1) && (cd->num_matched > 2))
    {
      /* cycle through all the matches */
      snprintf(cd->completed, sizeof(cd->completed), "%s",
               cd->match_list[(numtabs - 2) % cd->num_matched]);
    }

    /* return the completed query */
    strncpy(pt, cd->completed, buf->data + buf->dsize - pt - spaces);
  }
  else
  {
    return false;
  }

  return true;
}

/**
 * mutt_nm_tag_complete - Complete to the nearest notmuch tag
 * @param cd      Completion Data
 * @param buf     Buffer for the result
 * @param numtabs Number of times the user has hit 'tab'
 * @retval true  Success, a match
 * @retval false Error, no match
 *
 * Complete the nearest "+" or "-" -prefixed string previous to pos.
 */
bool mutt_nm_tag_complete(struct CompletionData *cd, struct Buffer *buf, int numtabs)
{
  if (!buf)
    return false;

  char *pt = buf->data;

  /* Only examine the last token */
  char *last_space = strrchr(buf->data, ' ');
  if (last_space)
    pt = (last_space + 1);

  /* Skip the +/- */
  if ((pt[0] == '+') || (pt[0] == '-'))
    pt++;

  if (numtabs == 1)
  {
    /* First TAB. Collect all the matches */
    complete_all_nm_tags(cd, pt);

    /* All matches are stored. Longest non-ambiguous string is ""
     * i.e. don't change 'buf'. Fake successful return this time.  */
    if (cd->user_typed[0] == '\0')
      return true;
  }

  if ((cd->completed[0] == '\0') && (cd->user_typed[0] != '\0'))
    return false;

  /* cd->num_matched will _always_ be at least 1 since the initial
   * user-typed string is always stored */
  if ((numtabs == 1) && (cd->num_matched == 2))
  {
    snprintf(cd->completed, sizeof(cd->completed), "%s", cd->match_list[0]);
  }
  else if ((numtabs > 1) && (cd->num_matched > 2))
  {
    /* cycle through all the matches */
    snprintf(cd->completed, sizeof(cd->completed), "%s",
             cd->match_list[(numtabs - 2) % cd->num_matched]);
  }

  /* return the completed query */
  strncpy(pt, cd->completed, buf->data + buf->dsize - pt);

  return true;
}

/**
 * complete_nm_query - Complete a Notmuch Query - Implements ::complete_function_t - @ingroup complete_api
 */
int complete_nm_query(struct EnterWindowData *wdata, int op)
{
  if (!wdata || ((op != OP_EDITOR_COMPLETE) && (op != OP_EDITOR_COMPLETE_QUERY)))
    return FR_NO_ACTION;

  int rc = FR_SUCCESS;
  buf_mb_wcstombs(wdata->buffer, wdata->state->wbuf, wdata->state->curpos);
  size_t len = buf_len(wdata->buffer);
  if (!mutt_nm_query_complete(wdata->cd, wdata->buffer, len, wdata->tabs))
    rc = FR_ERROR;

  replace_part(wdata->state, 0, buf_string(wdata->buffer));
  return rc;
}

/**
 * complete_nm_tag - Complete a Notmuch Tag - Implements ::complete_function_t - @ingroup complete_api
 */
int complete_nm_tag(struct EnterWindowData *wdata, int op)
{
  if (!wdata || ((op != OP_EDITOR_COMPLETE) && (op != OP_EDITOR_COMPLETE_QUERY)))
    return FR_NO_ACTION;

  int rc = FR_SUCCESS;
  buf_mb_wcstombs(wdata->buffer, wdata->state->wbuf, wdata->state->curpos);
  if (!mutt_nm_tag_complete(wdata->cd, wdata->buffer, wdata->tabs))
    rc = FR_ERROR;

  replace_part(wdata->state, 0, buf_string(wdata->buffer));
  return rc;
}

/**
 * CompleteNmQueryOps - Auto-Completion of NmQuerys
 */
const struct CompleteOps CompleteNmQueryOps = {
  .complete = complete_nm_query,
};

/**
 * CompleteNmTagOps - Auto-Completion of NmTags
 */
const struct CompleteOps CompleteNmTagOps = {
  .complete = complete_nm_tag,
};
