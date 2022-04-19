/**
 * @file
 * Mixmaster functions
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

#ifndef MUTT_MIXMASTER_FUNCTIONS_H
#define MUTT_MIXMASTER_FUNCTIONS_H

struct MixmasterPrivateData;

/**
 * @defgroup mixmaster_function_api Mixmaster Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Mixmaster Function
 *
 * @param priv  Mixmaster private data
 * @param op    Operation to perform, e.g. OP_MIXMASTER_NEXT
 * @retval enum #FunctionRetval
 */
typedef int (*mixmaster_function_t)(struct MixmasterPrivateData *priv, int op);

/**
 * struct MixmasterFunction - A NeoMutt function
 */
struct MixmasterFunction
{
  int op;                        ///< Op code, e.g. OP_MIX_USE
  mixmaster_function_t function; ///< Function to call
};

int mix_function_dispatcher(struct MuttWindow *win, int op);

extern struct MixmasterFunction MixmasterFunctions[];

#endif /* MUTT_MIXMASTER_FUNCTIONS_H */
