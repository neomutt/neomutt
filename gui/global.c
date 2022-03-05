/**
 * @file
 * Global functions
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
 * @page gui_global Global functions
 *
 * Global functions
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "global.h"
#include "lib.h"
#include "index/lib.h"
#include "muttlib.h"
#include "opcodes.h"

/**
 * op_version - Show the NeoMutt version number - Implements ::global_function_t - @ingroup global_function_api
 */
static int op_version(int op)
{
  mutt_message(mutt_make_version());
  return IR_SUCCESS;
}

/**
 * op_what_key - display the keycode for a key press - Implements ::global_function_t - @ingroup global_function_api
 */
static int op_what_key(int op)
{
  mutt_what_key();
  return IR_SUCCESS;
}

/**
 * GlobalFunctions - All the NeoMutt functions that the Global supports
 */
struct GlobalFunction GlobalFunctions[] = {
  // clang-format off
  { OP_VERSION,               op_version },
  { OP_WHAT_KEY,              op_what_key },
  { 0, NULL },
  // clang-format on
};

/**
 * global_function_dispatcher - Perform a Global function
 * @param win Window
 * @param op  Operation to perform, e.g. OP_VERSION
 * @retval num #IndexRetval, e.g. #IR_SUCCESS
 */
int global_function_dispatcher(struct MuttWindow *win, int op)
{
  int rc = IR_UNKNOWN;
  for (size_t i = 0; GlobalFunctions[i].op != OP_NULL; i++)
  {
    const struct GlobalFunction *fn = &GlobalFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(op);
      break;
    }
  }

  if (rc == IR_UNKNOWN) // Not our function
    return rc;

  const char *result = mutt_map_get_name(rc, RetvalNames);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return IR_SUCCESS; // Whatever the outcome, we handled it
}
