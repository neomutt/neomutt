/**
 * @file
 * Autocrypt functions
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

#ifndef MUTT_AUTOCRYPT_FUNCTIONS_H
#define MUTT_AUTOCRYPT_FUNCTIONS_H

struct AutocryptData;
struct KeyEvent;
struct MuttWindow;

/**
 * @defgroup autocrypt_function_api Autocrypt Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Autocrypt Function
 *
 * @param menu   Menu
 * @param op     Operation to perform, e.g. OP_AUTOCRYPT_CREATE_ACCT
 * @retval enum #FunctionRetval
 */
typedef int (*autocrypt_function_t)(struct AutocryptData *pd, int op);

/**
 * struct AutocryptFunction - A NeoMutt function
 */
struct AutocryptFunction
{
  int op;                        ///< Op code, e.g. OP_AUTOCRYPT_CREATE_ACCT
  autocrypt_function_t function; ///< Function to call
};

int autocrypt_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event);

#endif /* MUTT_AUTOCRYPT_FUNCTIONS_H */
