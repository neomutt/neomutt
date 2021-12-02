/**
 * @file
 * Browser functions
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

#ifndef MUTT_BROWSER_FUNCTIONS_H
#define MUTT_BROWSER_FUNCTIONS_H

#include <stdbool.h>

struct MuttWindow;
struct BrowserPrivateData;
struct BrowserView;

/**
 * @defgroup browser_function_api Browser Function API
 *
 * Prototype for a Browser Function
 *
 * @param priv   Private Browser data
 * @param op     Operation to perform, e.g. OP_MAIN_LIMIT
 * @retval enum #IndexRetval
 */
typedef int (*browser_function_t)(struct BrowserPrivateData *priv, int op);

/**
 * struct BrowserFunction - A NeoMutt function
 */
struct BrowserFunction
{
  int op;                      ///< Op code, e.g. OP_MAIN_LIMIT
  browser_function_t function; ///< Function to call
};

extern struct BrowserFunction BrowserFunctions[];

#endif //MUTT_BROWSER_FUNCTIONS_H
