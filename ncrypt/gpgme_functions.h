/**
 * @file
 * Gpgme functions
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

#ifndef MUTT_GPGME_FUNCTIONS_H
#define MUTT_GPGME_FUNCTIONS_H

#include <stdbool.h>

struct MuttWindow;

/**
 * struct GpgmeData - Data to pass to the Gpgme Functions
 */
struct GpgmeData
{
  bool done;                         ///< Should we close the Dialog?
  struct Menu *menu;                 ///< Gpgme Menu
  struct CryptKeyInfo **key_table;   ///< Array of Keys
  struct CryptKeyInfo *key;          ///< Selected Key
  bool *forced_valid;                ///< User insists on out-of-date key
};

/**
 * @defgroup gpgme_function_api Gpgme Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Gpgme Function
 *
 * @param menu   Menu
 * @param op     Operation to perform, e.g. OP_GENERIC_SELECT_ENTRY
 * @retval enum #FunctionRetval
 */
typedef int (*gpgme_function_t)(struct GpgmeData *gd, int op);

/**
 * struct GpgmeFunction - A NeoMutt function
 */
struct GpgmeFunction
{
  int op;                      ///< Op code, e.g. OP_GENERIC_SELECT_ENTRY
  gpgme_function_t function; ///< Function to call
};

extern struct GpgmeFunction GpgmeFunctions[];

int gpgme_function_dispatcher(struct MuttWindow *win, int op);

#endif /* MUTT_GPGME_FUNCTIONS_H */
