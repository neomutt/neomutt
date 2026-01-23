/**
 * @file
 * Pattern Auto-Completion
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
 * @page pattern_complete Pattern Auto-Completion
 *
 * Pattern Auto-Completion
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "complete/lib.h"
#include "editor/lib.h"

/**
 * is_pattern_prefix - Check if a character is a pattern prefix
 * @param c Character to check
 * @retval true  Character is a pattern prefix (~, %, or =)
 * @retval false Character is not a pattern prefix
 */
static bool is_pattern_prefix(wchar_t c)
{
  return (c == '~') || (c == '%') || (c == '=');
}

/**
 * complete_pattern - Complete a NeoMutt Pattern - Implements CompleteOps::complete() - @ingroup compapi_complete
 */
static enum FunctionRetval complete_pattern(struct EnterWindowData *wdata, int op)
{
  if (!wdata || ((op != OP_EDITOR_COMPLETE) && (op != OP_EDITOR_COMPLETE_QUERY)))
    return FR_NO_ACTION;

  size_t i = wdata->state->curpos;

  // Check if cursor is right after a pattern prefix (~, %, or =)
  if (i && is_pattern_prefix(wdata->state->wbuf[i - 1]))
  {
    if (dlg_pattern(wdata->buffer))
      replace_part(wdata->state, i - 1, wdata->buffer->data);
    buf_fix_dptr(wdata->buffer);
    return FR_CONTINUE;
  }

  // Search backwards for a pattern prefix
  for (; (i > 0) && !is_pattern_prefix(wdata->state->wbuf[i - 1]); i--)
    ; // do nothing

  if ((i > 0) && (i < wdata->state->curpos) &&
      (wdata->state->wbuf[i - 1] == '~') && (wdata->state->wbuf[i] == 'y'))
  {
    // Label completion for ~y pattern
    i++;
    buf_mb_wcstombs(wdata->buffer, wdata->state->wbuf + i, wdata->state->curpos - i);
    int rc = mutt_label_complete(wdata->cd, wdata->buffer, wdata->tabs);
    replace_part(wdata->state, i, wdata->buffer->data);
    buf_fix_dptr(wdata->buffer);
    if (rc != 1)
    {
      return FR_CONTINUE;
    }
  }
  else
  {
    return FR_NO_ACTION;
  }

  return FR_SUCCESS;
}

/**
 * CompletePatternOps - Auto-Completion of Patterns
 */
const struct CompleteOps CompletePatternOps = {
  .complete = complete_pattern,
};
