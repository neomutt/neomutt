/**
 * @file
 * Alias Auto-Completion
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
 * @page alias_complete Alias Auto-Completion
 *
 * Alias Auto-Completion
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
 * complete_alias_complete - Complete an Alias - Implements ::complete_function_t - @ingroup complete_api
 */
int complete_alias_complete(struct EnterWindowData *wdata, int op)
{
  if (!wdata || (op != OP_EDITOR_COMPLETE))
    return FR_NO_ACTION;

  /* invoke the alias-menu to get more addresses */
  size_t i;
  for (i = wdata->state->curpos; (i > 0) && (wdata->state->wbuf[i - 1] != ',') &&
                                 (wdata->state->wbuf[i - 1] != ':');
       i--)
  {
  }
  for (; (i < wdata->state->lastchar) && (wdata->state->wbuf[i] == ' '); i++)
    ; // do nothing

  buf_mb_wcstombs(wdata->buffer, wdata->state->wbuf + i, wdata->state->curpos - i);
  int rc = alias_complete(wdata->buffer, NeoMutt->sub);
  replace_part(wdata->state, i, buf_string(wdata->buffer));
  if (rc != 1)
  {
    return FR_CONTINUE;
  }

  return FR_SUCCESS;
}

/**
 * complete_alias_query - Complete an Alias Query - Implements ::complete_function_t - @ingroup complete_api
 */
int complete_alias_query(struct EnterWindowData *wdata, int op)
{
  if (!wdata || (op != OP_EDITOR_COMPLETE_QUERY))
    return FR_NO_ACTION;

  size_t i = wdata->state->curpos;
  if (i != 0)
  {
    for (; (i > 0) && (wdata->state->wbuf[i - 1] != ','); i--)
      ; // do nothing

    for (; (i < wdata->state->curpos) && (wdata->state->wbuf[i] == ' '); i++)
      ; // do nothing
  }

  buf_mb_wcstombs(wdata->buffer, wdata->state->wbuf + i, wdata->state->curpos - i);
  query_complete(wdata->buffer, NeoMutt->sub);
  replace_part(wdata->state, i, buf_string(wdata->buffer));

  return FR_CONTINUE;
}

/**
 * complete_alias - Alias completion wrapper - Implements ::complete_function_t - @ingroup complete_api
 */
int complete_alias(struct EnterWindowData *wdata, int op)
{
  if (op == OP_EDITOR_COMPLETE)
    return complete_alias_complete(wdata, op);
  if (op == OP_EDITOR_COMPLETE_QUERY)
    return complete_alias_query(wdata, op);

  return FR_NO_ACTION;
}

/**
 * CompleteAliasOps - Auto-Completion of Aliases
 */
const struct CompleteOps CompleteAliasOps = {
  .complete = complete_alias,
};
