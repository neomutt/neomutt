/**
 * @file
 * Pgp functions
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

#ifndef MUTT_NCRYPT_PGP_FUNCTIONS_H
#define MUTT_NCRYPT_PGP_FUNCTIONS_H

#include <stdbool.h>

struct MuttWindow;

/**
 * struct PgpData - Data to pass to the Pgp Functions
 */
struct PgpData
{
  bool done;                   ///< Should we close the Dialog?
  struct Menu *menu;           ///< Pgp Menu
  struct PgpUid **key_table;   ///< Array of Keys
  struct PgpKeyInfo *key;      ///< Selected Key
};

/**
 * @defgroup pgp_function_api Pgp Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Pgp Function
 *
 * @param menu   Menu
 * @param op     Operation to perform, e.g. OP_GENERIC_SELECT_ENTRY
 * @retval enum #FunctionRetval
 */
typedef int (*pgp_function_t)(struct PgpData *pd, int op);

/**
 * struct PgpFunction - A NeoMutt function
 */
struct PgpFunction
{
  int op;                      ///< Op code, e.g. OP_GENERIC_SELECT_ENTRY
  pgp_function_t function; ///< Function to call
};

int pgp_function_dispatcher(struct MuttWindow *win, int op);

#endif /* MUTT_NCRYPT_PGP_FUNCTIONS_H */
