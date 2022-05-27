/**
 * @file
 * Enter functions
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

#ifndef MUTT_ENTER_FUNCTIONS_H
#define MUTT_ENTER_FUNCTIONS_H

#include <stdbool.h>

struct EnterWindowData;
struct MuttWindow;

/**
 * @defgroup enter_function_api Enter Function API
 *
 * Prototype for a Enter Function
 *
 * @param wdata  Enter Window data
 * @param op     Operation to perform, e.g. OP_ENTER_NEXT
 * @retval enum #FunctionRetval
 */
typedef int (*enter_function_t)(struct EnterWindowData *wdata, int op);

/**
 * struct EnterFunction - A NeoMutt function
 */
struct EnterFunction
{
  int op;                    ///< Op code, e.g. OP_SEARCH
  enter_function_t function; ///< Function to call
};

extern struct EnterFunction EnterFunctions[];

int enter_function_dispatcher(struct MuttWindow *win, int op);
bool self_insert(struct EnterWindowData *wdata, int ch);

#endif /* MUTT_ENTER_FUNCTIONS_H */
