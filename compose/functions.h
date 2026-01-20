/**
 * @file
 * Compose functions
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

#ifndef MUTT_COMPOSE_FUNCTIONS_H
#define MUTT_COMPOSE_FUNCTIONS_H

struct ComposeSharedData;
struct KeyEvent;
struct MuttWindow;

/**
 * @defgroup compose_function_api Compose Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Compose Function
 *
 * @param shared Shared Compose data
 * @param event  Event to process
 * @retval enum #FunctionRetval
 *
 * @pre shared is not NULL
 * @pre event  is not NULL
 */
typedef int (*compose_function_t)(struct ComposeSharedData *priv, const struct KeyEvent *event);

/**
 * struct ComposeFunction - A NeoMutt function
 */
struct ComposeFunction
{
  int op;                      ///< Op code, e.g. OP_COMPOSE_WRITE_MESSAGE
  compose_function_t function; ///< Function to call
};

int compose_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event);

#endif /* MUTT_COMPOSE_FUNCTIONS_H */
