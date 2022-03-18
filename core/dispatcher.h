/**
 * @file
 * Dispatcher of functions
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

#ifndef MUTT_CORE_DISPATCHER_H
#define MUTT_CORE_DISPATCHER_H

struct MuttWindow;

/**
 * enum FunctionRetval - Possible return values for NeoMutt functions
 */
enum FunctionRetval
{
  FR_UNKNOWN   = -7, ///< Unknown function
  FR_CONTINUE  = -6, ///< Remain in the Dialog
  FR_DONE      = -5, ///< Exit the Dialog
  FR_NOT_IMPL  = -4, ///< Invalid function - feature not enabled
  FR_NO_ACTION = -3, ///< Valid function - no action performed
  FR_ERROR     = -2, ///< Valid function - error occurred
  FR_SUCCESS   = -1, ///< Valid function - successfully performed
};

/**
 * @defgroup dispatcher_api Function Dispatcher API
 *
 * Prototype for a Function Dispatcher
 *
 * function_dispatcher_t - Perform a NeoMutt function
 * @param win Window
 * @param op  Operation to perform, e.g. OP_MAIN_LIMIT
 * @retval num FunctionRetval
 */
typedef int (*function_dispatcher_t)(struct MuttWindow *win, int op);

const char *dispacher_get_retval_name(int rv);

#endif /* MUTT_CORE_DISPATCHER_H */
