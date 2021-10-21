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

struct IndexSharedData;
struct MuttWindow;
struct PagerPrivateData;
struct PagerView;

/**
 * @defgroup pager_function_api Pager Function API
 *
 * Prototype for a Pager Function
 *
 * @param shared Shared Index data
 * @param priv   Private Index data
 * @param op     Operation to perform, e.g. OP_MAIN_LIMIT
 * @retval enum #IndexRetval
 */
typedef int (*pager_function_t)(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op);

/**
 * struct PagerFunction - A NeoMutt function
 */
struct PagerFunction
{
  int op;                    ///< Op code, e.g. OP_MAIN_LIMIT
  pager_function_t function; ///< Function to call
};

int pager_function_dispatcher(struct MuttWindow *win_index, int op);
bool jump_to_bottom(struct PagerPrivateData *priv, struct PagerView *pview);

extern struct PagerFunction PagerFunctions[];

#endif //MUTT_PAGER_FUNCTIONS_H
