/**
 * @file
 * Sidebar functions
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

#ifndef MUTT_SIDEBAR_FUNCTIONS_H
#define MUTT_SIDEBAR_FUNCTIONS_H

struct SidebarWindowData;

/**
 * @defgroup sidebar_function_api Sidebar Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Sidebar Function
 *
 * @param wdata  Sidebar Window data
 * @param op     Operation to perform, e.g. OP_SIDEBAR_NEXT
 * @retval enum #FunctionRetval
 */
typedef int (*sidebar_function_t)(struct SidebarWindowData *wdata, int op);

/**
 * struct SidebarFunction - A NeoMutt function
 */
struct SidebarFunction
{
  int op;                      ///< Op code, e.g. OP_SIDEBAR_NEXT
  sidebar_function_t function; ///< Function to call
};

extern struct SidebarFunction SidebarFunctions[];

#endif /* MUTT_SIDEBAR_FUNCTIONS_H */
