/**
 * @file
 * Auto-completion helpers
 *
 * @authors
 * Copyright (C) 2022-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Anna Figueiredo Gomes <navi@vlhl.dev>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * @page complete_helpers Auto-completion helpers
 *
 * Auto-completion helpers
 */

#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "editor/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "compapi.h"
#include "data.h"
#include "mview.h"
#include "sort.h"

/**
 * matches_ensure_morespace - Allocate more space for auto-completion
 * @param cd       Completion Data
 * @param new_size Space required
 */
void matches_ensure_morespace(struct CompletionData *cd, int new_size)
{
  if (new_size <= (cd->match_list_len - 2))
    return;

  new_size = ROUND_UP(new_size + 2, 512);

  mutt_mem_realloc(&cd->match_list, new_size * sizeof(char *));
  memset(&cd->match_list[cd->match_list_len], 0, new_size - cd->match_list_len);

  cd->match_list_len = new_size;
}

/**
 * candidate - Helper function for completion
 * @param cd   Completion Data
 * @param user User entered data for completion
 * @param src  Candidate for completion
 * @param dest Completion result gets here
 * @param dlen Length of dest buffer
 * @retval true If candidate string matches
 *
 * Changes the dest buffer if necessary/possible to aid completion.
 */
bool candidate(struct CompletionData *cd, char *user, const char *src, char *dest, size_t dlen)
{
  if (!dest || !user || !src)
    return false;

  if (strstr(src, user) != src)
    return false;

  matches_ensure_morespace(cd, cd->num_matched);
  cd->match_list[cd->num_matched++] = src;
  if (dest[0] == '\0')
  {
    mutt_str_copy(dest, src, dlen);
  }
  else
  {
    int l;
    for (l = 0; (src[l] != '\0') && (src[l] == dest[l]); l++)
      ; // do nothing

    dest[l] = '\0';
  }
  return true;
}

/**
 * mutt_command_complete - Complete a command name
 * @param cd      Completion Data
 * @param buf     Buffer for the result
 * @param pos     Cursor position in the buffer
 * @param numtabs Number of times the user has hit 'tab'
 * @retval 1 Success, a match
 * @retval 0 Error, no match
 */
int mutt_command_complete(struct CompletionData *cd, struct Buffer *buf, int pos, int numtabs)
{
  char *pt = buf->data;
  int spaces; /* keep track of the number of leading spaces on the line */

  SKIPWS(pt);
  spaces = pt - buf->data;

  pt = buf->data + pos - spaces;
  while ((pt > buf->data) && !isspace((unsigned char) *pt))
    pt--;

  if (pt == buf->data) /* complete cmd */
  {
    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      cd->num_matched = 0;
      mutt_str_copy(cd->user_typed, pt, sizeof(cd->user_typed));
      memset(cd->match_list, 0, cd->match_list_len);
      memset(cd->completed, 0, sizeof(cd->completed));

      struct Command *c = NULL;
      for (size_t num = 0, size = commands_array(&c); num < size; num++)
        candidate(cd, cd->user_typed, c[num].name, cd->completed, sizeof(cd->completed));
      matches_ensure_morespace(cd, cd->num_matched);
      cd->match_list[cd->num_matched++] = cd->user_typed;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time */
      if (cd->user_typed[0] == '\0')
        return 1;
    }

    if ((cd->completed[0] == '\0') && (cd->user_typed[0] != '\0'))
      return 0;

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

    /* return the completed command */
    buf_strcpy(buf, cd->completed);
  }
  else if (buf_startswith(buf, "set") || buf_startswith(buf, "unset") ||
           buf_startswith(buf, "reset") || buf_startswith(buf, "toggle"))
  { /* complete variables */
    static const char *const prefixes[] = { "no", "inv", "?", "&", 0 };

    pt++;
    /* loop through all the possible prefixes (no, inv, ...) */
    if (buf_startswith(buf, "set"))
    {
      for (int num = 0; prefixes[num]; num++)
      {
        if (mutt_str_startswith(pt, prefixes[num]))
        {
          pt += mutt_str_len(prefixes[num]);
          break;
        }
      }
    }

    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      cd->num_matched = 0;
      mutt_str_copy(cd->user_typed, pt, sizeof(cd->user_typed));
      memset(cd->match_list, 0, cd->match_list_len);
      memset(cd->completed, 0, sizeof(cd->completed));

      struct HashElem *he = NULL;
      struct HashElem **he_list = get_elem_list(NeoMutt->sub->cs);
      for (size_t i = 0; he_list[i]; i++)
      {
        he = he_list[i];
        const int type = DTYPE(he->type);

        if ((type == DT_SYNONYM) || (type & D_INTERNAL_DEPRECATED))
          continue;

        candidate(cd, cd->user_typed, he->key.strkey, cd->completed, sizeof(cd->completed));
      }
      FREE(&he_list);

      matches_ensure_morespace(cd, cd->num_matched);
      cd->match_list[cd->num_matched++] = cd->user_typed;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time */
      if (cd->user_typed[0] == '\0')
        return 1;
    }

    if ((cd->completed[0] == 0) && cd->user_typed[0])
      return 0;

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

    strncpy(pt, cd->completed, buf->data + buf->dsize - pt - spaces);
    buf_fix_dptr(buf);
  }
  else if (buf_startswith(buf, "exec"))
  {
    const enum MenuType mtype = menu_get_current_type();
    const struct MenuFuncOp *funcs = km_get_table(mtype);
    if (!funcs && (mtype != MENU_PAGER))
      funcs = OpGeneric;

    pt++;
    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      cd->num_matched = 0;
      mutt_str_copy(cd->user_typed, pt, sizeof(cd->user_typed));
      memset(cd->match_list, 0, cd->match_list_len);
      memset(cd->completed, 0, sizeof(cd->completed));
      for (int num = 0; funcs[num].name; num++)
        candidate(cd, cd->user_typed, funcs[num].name, cd->completed, sizeof(cd->completed));
      /* try the generic menu */
      if ((mtype != MENU_PAGER) && (mtype != MENU_GENERIC))
      {
        funcs = OpGeneric;
        for (int num = 0; funcs[num].name; num++)
          candidate(cd, cd->user_typed, funcs[num].name, cd->completed,
                    sizeof(cd->completed));
      }
      matches_ensure_morespace(cd, cd->num_matched);
      cd->match_list[cd->num_matched++] = cd->user_typed;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time */
      if (cd->user_typed[0] == '\0')
        return 1;
    }

    if ((cd->completed[0] == '\0') && (cd->user_typed[0] != '\0'))
      return 0;

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

    strncpy(pt, cd->completed, buf->data + buf->dsize - pt - spaces);
    buf_fix_dptr(buf);
  }
  else
  {
    return 0;
  }

  return 1;
}

/**
 * label_sort - Compare two label strings - Implements ::sort_t - @ingroup sort_api
 */
static int label_sort(const void *a, const void *b, void *sdata)
{
  return strcasecmp(*(const char **) a, *(const char **) b);
}

/**
 * mutt_label_complete - Complete a label name
 * @param cd      Completion Data
 * @param buf     Buffer for the result
 * @param numtabs Number of times the user has hit 'tab'
 * @retval 1 Success, a match
 * @retval 0 Error, no match
 */
int mutt_label_complete(struct CompletionData *cd, struct Buffer *buf, int numtabs)
{
  char *pt = buf->data;

  struct Mailbox *m_cur = get_current_mailbox();
  if (!m_cur || !m_cur->label_hash)
    return 0;

  SKIPWS(pt);

  /* first TAB. Collect all the matches */
  if (numtabs == 1)
  {
    struct HashElem *he = NULL;
    struct HashWalkState hws = { 0 };

    cd->num_matched = 0;
    mutt_str_copy(cd->user_typed, buf_string(buf), sizeof(cd->user_typed));
    memset(cd->match_list, 0, cd->match_list_len);
    memset(cd->completed, 0, sizeof(cd->completed));
    while ((he = mutt_hash_walk(m_cur->label_hash, &hws)))
      candidate(cd, cd->user_typed, he->key.strkey, cd->completed, sizeof(cd->completed));
    matches_ensure_morespace(cd, cd->num_matched);
    mutt_qsort_r(cd->match_list, cd->num_matched, sizeof(char *), label_sort, NULL);
    cd->match_list[cd->num_matched++] = cd->user_typed;

    /* All matches are stored. Longest non-ambiguous string is ""
     * i.e. don't change 'buf'. Fake successful return this time */
    if (cd->user_typed[0] == '\0')
      return 1;
  }

  if ((cd->completed[0] == '\0') && (cd->user_typed[0] != '\0'))
    return 0;

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

  /* return the completed label */
  buf_strcpy(buf, cd->completed);

  return 1;
}

/**
 * mutt_var_value_complete - Complete a variable/value
 * @param cd  Completion Data
 * @param buf Buffer for the result
 * @param pos Cursor position in the buffer
 * @retval 1 Success
 * @retval 0 Failure
 */
int mutt_var_value_complete(struct CompletionData *cd, struct Buffer *buf, int pos)
{
  char *pt = buf->data;

  if (pt[0] == '\0')
    return 0;

  SKIPWS(pt);
  const int spaces = pt - buf->data;

  pt = buf->data + pos - spaces;
  while ((pt > buf->data) && !isspace((unsigned char) *pt))
    pt--;
  pt++;           /* move past the space */
  if (*pt == '=') /* abort if no var before the '=' */
    return 0;

  if (buf_startswith(buf, "set"))
  {
    char var[256] = { 0 };
    mutt_str_copy(var, pt, sizeof(var));
    /* ignore the trailing '=' when comparing */
    int vlen = mutt_str_len(var);
    if (vlen == 0)
      return 0;

    var[vlen - 1] = '\0';

    struct HashElem *he = cs_subset_lookup(NeoMutt->sub, var);
    if (!he)
      return 0; /* no such variable. */

    struct Buffer *value = buf_pool_get();
    struct Buffer *pretty = buf_pool_get();
    int rc = cs_subset_he_string_get(NeoMutt->sub, he, value);
    if (CSR_RESULT(rc) == CSR_SUCCESS)
    {
      pretty_var(value->data, pretty);
      snprintf(pt, buf->dsize - (pt - buf->data), "%s=%s", var, pretty->data);
      buf_pool_release(&value);
      buf_pool_release(&pretty);
      return 0;
    }
    buf_pool_release(&value);
    buf_pool_release(&pretty);
    return 1;
  }
  return 0;
}

/**
 * complete_command - Complete a NeoMutt Command - Implements ::complete_function_t - @ingroup complete_api
 */
int complete_command(struct EnterWindowData *wdata, int op)
{
  if (!wdata || ((op != OP_EDITOR_COMPLETE) && (op != OP_EDITOR_COMPLETE_QUERY)))
    return FR_NO_ACTION;

  int rc = FR_SUCCESS;
  buf_mb_wcstombs(wdata->buffer, wdata->state->wbuf, wdata->state->curpos);
  size_t i = buf_len(wdata->buffer);
  if ((i != 0) && (buf_at(wdata->buffer, i - 1) == '=') &&
      (mutt_var_value_complete(wdata->cd, wdata->buffer, i) != 0))
  {
    wdata->tabs = 0;
  }
  else if (mutt_command_complete(wdata->cd, wdata->buffer, i, wdata->tabs) == 0)
  {
    rc = FR_ERROR;
  }

  replace_part(wdata->state, 0, buf_string(wdata->buffer));
  return rc;
}

/**
 * complete_label - Complete a label - Implements ::complete_function_t - @ingroup complete_api
 */
int complete_label(struct EnterWindowData *wdata, int op)
{
  if (!wdata || ((op != OP_EDITOR_COMPLETE) && (op != OP_EDITOR_COMPLETE_QUERY)))
    return FR_NO_ACTION;

  size_t i;
  for (i = wdata->state->curpos; (i > 0) && (wdata->state->wbuf[i - 1] != ',') &&
                                 (wdata->state->wbuf[i - 1] != ':');
       i--)
  {
  }
  for (; (i < wdata->state->lastchar) && (wdata->state->wbuf[i] == ' '); i++)
    ; // do nothing

  buf_mb_wcstombs(wdata->buffer, wdata->state->wbuf + i, wdata->state->curpos - i);
  int rc = mutt_label_complete(wdata->cd, wdata->buffer, wdata->tabs);
  replace_part(wdata->state, i, buf_string(wdata->buffer));
  if (rc != 1)
    return FR_CONTINUE;

  return FR_SUCCESS;
}

/**
 * CompleteCommandOps - Auto-Completion of Commands
 */
const struct CompleteOps CompleteCommandOps = {
  .complete = complete_command,
};

/**
 * CompleteLabelOps - Auto-Completion of Labels
 */
const struct CompleteOps CompleteLabelOps = {
  .complete = complete_label,
};
