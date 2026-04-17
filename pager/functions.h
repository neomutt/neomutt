/**
 * @file
 * Pager functions
 *
 * @authors
 * Copyright (C) 2021-2022 Richard Russon <rich@flatcap.org>
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

struct KeyEvent;
struct MuttWindow;
struct PagerPrivateData;
struct PagerView;

/**
 * struct PagerFunctionData - Data passed to Pager worker functions
 */
struct PagerFunctionData
{
  struct NeoMutt *n;                ///< NeoMutt application data
  struct PagerModuleData *mod_data; ///< Pager module data
  struct IndexSharedData *shared;   ///< Shared Index data
  struct PagerPrivateData *priv;    ///< Private Pager data
};

/**
 * @defgroup pager_function_api Pager Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Pager Function
 *
 * @param fdata   Pager Function context data
 * @param event Event to process
 * @retval enum #FunctionRetval
 *
 * @pre fdata   is not NULL
 * @pre event is not NULL
 */
typedef int (*pager_function_t)(struct PagerFunctionData *fdata,
                                const struct KeyEvent *event);

/**
 * struct PagerFunction - A NeoMutt function
 */
struct PagerFunction
{
  int op;                    ///< Op code, e.g. OP_MAIN_LIMIT
  pager_function_t function; ///< Function to call
};

int pager_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event);
bool jump_to_bottom(struct PagerPrivateData *priv, struct PagerView *pview);
struct MenuDefinition *pager_get_menu_definition(void);

#define MdPager (pager_get_menu_definition())

#endif /* MUTT_PAGER_FUNCTIONS_H */
