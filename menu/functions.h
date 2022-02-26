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

#ifndef MUTT_MENU_FUNCTIONS_H
#define MUTT_MENU_FUNCTIONS_H

#include <stdbool.h>

struct Menu;

/**
 * @defgroup menu_function_api Menu Function API
 *
 * Prototype for a Menu Function
 *
 * @param menu Menu
 * @param op   Operation to perform, e.g. OP_NEXT_PAGE
 * @retval enum #IndexRetval
 */
typedef int (*menu_function_t)(struct Menu *menu, int op);

/**
 * struct MenuFunction - A NeoMutt function
 */
struct MenuFunction
{
  int op;                    ///< Op code, e.g. OP_SEARCH
  menu_function_t function; ///< Function to call
};

extern struct MenuFunction MenuFunctions[];

#endif /* MUTT_MENU_FUNCTIONS_H */
