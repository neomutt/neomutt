/**
 * @file
 * Menu functions
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
 * @page menu_functions Menu functions
 *
 * Menu functions
 */

#include "config.h"
#include "mutt/lib.h"
#include "gui/lib.h"
#include "functions.h"
#include "lib.h"
#include "index/lib.h"
#include "mutt_logging.h"
#include "opcodes.h"

int menu_dialog_dokey(struct Menu *menu, int *ip);
int menu_dialog_translate_op(int i);

// -----------------------------------------------------------------------------

/**
 * MenuFunctions - All the NeoMutt functions that the Menu supports
 */
struct MenuFunction MenuFunctions[] = {
  // clang-format off
  { 0, NULL },
  // clang-format on
};

/**
 * menu_function_dispatcher - Perform a Menu function
 * @param win Menu Window
 * @param op  Operation to perform, e.g. OP_MENU_NEXT
 * @retval num #IndexRetval, e.g. #IR_SUCCESS
 */
int menu_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return IR_UNKNOWN;

  struct Menu *menu = win->wdata;

  int rc = IR_UNKNOWN;
  for (size_t i = 0; MenuFunctions[i].op != OP_NULL; i++)
  {
    const struct MenuFunction *fn = &MenuFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(menu, op);
      break;
    }
  }

  if (rc == IR_UNKNOWN) // Not our function
    return rc;

  const char *result = mutt_map_get_name(rc, RetvalNames);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
