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

#ifndef MUTT_PAGER_FUNCTIONS_H
#define MUTT_PAGER_FUNCTIONS_H

#include <stdbool.h>

struct PagerPrivateData;
struct MuttWindow;

/**
 * enum PagerRetval - Possible return values for Pager functions
 */
enum PagerRetval
{
  IR_ERROR   = -2,
  IR_WARNING = -1,
  IR_SUCCESS =  0,
  IR_NOT_IMPL,
  IR_NO_ACTION,
  IR_VOID,
  IR_CONTINUE,
  IR_BREAK,
};

/**
 * typedef pager_function_t - Perform an Index Function
 * @param priv   Private Index data
 * @param op     Operation to perform, e.g. OP_MAIN_LIMIT
 * @retval enum #IndexRetval
 */
typedef enum PagerRetval (*pager_function_t)(struct PagerPrivateData *priv, int op);

struct PagerFunction
{
  int op;                    ///< Op code, e.g. OP_MAIN_LIMIT
  pager_function_t function; ///< Function to call
  // TODO not sure yet if pager needs this
  //int flags;               ///< Prerequisites for the function, e.g. #CHECK_IN_MAILBOX
};

bool pager_function_dispatcher(struct MuttWindow *win_index, int op);

extern struct PagerFunction PagerFunctions[];

#endif //MUTT_PAGER_FUNCTIONS_H
