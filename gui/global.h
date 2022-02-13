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

#ifndef MUTT_GUI_GLOBAL_H
#define MUTT_GUI_GLOBAL_H

struct MuttWindow;

/**
 * @defgroup global_function_api Global Function API
 *
 * Prototype for a Global Function
 *
 * @param op Operation to perform, e.g. OP_VERSION
 * @retval enum #IndexRetval
 */
typedef int (*global_function_t)(int op);

/**
 * struct GlobalFunction - A NeoMutt function
 */
struct GlobalFunction
{
  int op;                     ///< Op code, e.g. OP_GLOBAL_NEXT
  global_function_t function; ///< Function to call
};

extern struct GlobalFunction GlobalFunctions[];

int global_function_dispatcher(struct MuttWindow *win, int op);

#endif /* MUTT_GLOBAL_FUNCTIONS_H */
