/**
 * @file
 * Pager functions
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page pager_functions Pager functions
 *
 * Pager functions
 */

#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include "functions.h"

/**
 * pager_function_dispatcher - Perform a Pager function
 * @param win_pager Window for the Index
 * @param op        Operation to perform, e.g. OP_MAIN_LIMIT
 * @retval true Value function
 */
bool pager_function_dispatcher(struct MuttWindow *win_pager, int op)
{
  // TODO: write pager_function_dispatcher
  assert(false);
  return true;
}

/**
 * PagerFunctions - All the NeoMutt functions that the Pager supports
 */
struct PagerFunction PagerFunctions[] = {
  // clang-format off

  // clang-format on
  { 0, NULL },
};
