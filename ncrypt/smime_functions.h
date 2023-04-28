/**
 * @file
 * Smime functions
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

#ifndef MUTT_NCRYPT_SMIME_FUNCTIONS_H
#define MUTT_NCRYPT_SMIME_FUNCTIONS_H

#include <stdbool.h>

struct MuttWindow;

/**
 * struct SmimeData - Data to pass to the Smime Functions
 */
struct SmimeData
{
  bool done;                 ///< Should we close the Dialog?
  struct Menu *menu;         ///< Smime Menu
  struct SmimeKey **table;   ///< Array of Keys
  struct SmimeKey *key;      ///< Selected Key
};

/**
 * @defgroup smime_function_api Smime Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Smime Function
 *
 * @param menu   Menu
 * @param op     Operation to perform, e.g. OP_GENERIC_SELECT_ENTRY
 * @retval enum #FunctionRetval
 */
typedef int (*smime_function_t)(struct SmimeData *sd, int op);

/**
 * struct SmimeFunction - A NeoMutt function
 */
struct SmimeFunction
{
  int op;                    ///< Op code, e.g. OP_GENERIC_SELECT_ENTRY
  smime_function_t function; ///< Function to call
};

int smime_function_dispatcher(struct MuttWindow *win, int op);

#endif /* MUTT_NCRYPT_SMIME_FUNCTIONS_H */
