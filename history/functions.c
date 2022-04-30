/**
 * @file
 * History functions
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page history_functions History functions
 *
 * History functions
 */

#include "config.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "functions.h"
#include "menu/lib.h"
#include "opcodes.h"

/**
 * op_exit - Exit this menu - Implements ::history_function_t - @ingroup history_function_api
 */
static int op_exit(struct HistoryData *hd, int op)
{
  hd->done = true;
  hd->selection = false;
  return FR_SUCCESS;
}

/**
 * op_generic_select_entry - Select the current entry - Implements ::history_function_t - @ingroup history_function_api
 */
static int op_generic_select_entry(struct HistoryData *hd, int op)
{
  const int index = menu_get_index(hd->menu);
  mutt_str_copy(hd->buf, hd->matches[index], hd->buflen);

  hd->done = true;
  hd->selection = true;
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * HistoryFunctions - All the NeoMutt functions that the History supports
 */
struct HistoryFunction HistoryFunctions[] = {
  // clang-format off
  { OP_EXIT,                   op_exit },
  { OP_GENERIC_SELECT_ENTRY,   op_generic_select_entry },
  { 0, NULL },
  // clang-format on
};

/**
 * history_function_dispatcher - Perform a History function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int history_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return FR_UNKNOWN;

  struct MuttWindow *dlg = dialog_find(win);
  if (!dlg)
    return FR_ERROR;

  struct HistoryData *hd = dlg->wdata;

  int rc = FR_UNKNOWN;
  for (size_t i = 0; HistoryFunctions[i].op != OP_NULL; i++)
  {
    const struct HistoryFunction *fn = &HistoryFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(hd, op);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispacher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
