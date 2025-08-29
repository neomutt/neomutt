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

#ifndef MUTT_HISTORY_FUNCTIONS_H
#define MUTT_HISTORY_FUNCTIONS_H

#include <stdbool.h>
#include "mutt/lib.h"

struct MuttWindow;

ARRAY_HEAD(HistoryArray, const char *);

/**
 * struct HistoryData - Data to pass to the History Functions
 */
struct HistoryData
{
  bool done;                     ///< Should we close the Dialog?
  bool selection;                ///< Was a selection made?
  struct Buffer *buf;            ///< Buffer for the results
  struct Menu *menu;             ///< History Menu
  struct HistoryArray *matches;  ///< History entries
};

/**
 * @defgroup history_function_api History Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a History Function
 *
 * @param menu   Menu
 * @param op     Operation to perform, e.g. OP_GENERIC_SELECT_ENTRY
 * @retval enum #FunctionRetval
 */
typedef int (*history_function_t)(struct HistoryData *pd, int op);

/**
 * struct HistoryFunction - A NeoMutt function
 */
struct HistoryFunction
{
  int op;                      ///< Op code, e.g. OP_GENERIC_SELECT_ENTRY
  history_function_t function; ///< Function to call
};

int history_function_dispatcher(struct MuttWindow *win, int op);

#endif /* MUTT_HISTORY_FUNCTIONS_H */
