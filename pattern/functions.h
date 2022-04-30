/**
 * @file
 * Pattern functions
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

#ifndef MUTT_PATTERN_FUNCTIONS_H
#define MUTT_PATTERN_FUNCTIONS_H

#include <stdbool.h>
#include <stdio.h>

struct MuttWindow;

/**
 * struct PatternData - Data to pass to the Pattern Functions
 */
struct PatternData
{
  bool done;           ///< Should we close the Dialog?
  bool selection;      ///< Was a selection made?
  char *buf;           ///< Buffer for the results
  size_t buflen;       ///< Length of the results buffer
  struct Menu *menu;   ///< Pattern Menu
};

/**
 * @defgroup pattern_function_api Pattern Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Pattern Function
 *
 * @param menu   Menu
 * @param op     Operation to perform, e.g. OP_GENERIC_SELECT_ENTRY
 * @retval enum #FunctionRetval
 */
typedef int (*pattern_function_t)(struct PatternData *pd, int op);

/**
 * struct PatternFunction - A NeoMutt function
 */
struct PatternFunction
{
  int op;                      ///< Op code, e.g. OP_GENERIC_SELECT_ENTRY
  pattern_function_t function; ///< Function to call
};

extern struct PatternFunction PatternFunctions[];

int pattern_function_dispatcher(struct MuttWindow *win, int op);

#endif /* MUTT_PATTERN_FUNCTIONS_H */
