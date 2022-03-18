/**
 * @file
 * Alias functions
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

#ifndef MUTT_ALIAS_FUNCTIONS_H
#define MUTT_ALIAS_FUNCTIONS_H

#include <stdbool.h>

struct AddressList;
struct Alias;
struct AliasList;
struct AliasMenuData;
struct AliasViewArray;
struct ConfigSubset;
struct MuttWindow;

/**
 * @defgroup alias_function_api Alias Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Alias Function
 *
 * @param wdata  Alias Window data
 * @param op     Operation to perform, e.g. OP_ALIAS_NEXT
 * @retval enum #FunctionRetval
 */
typedef int (*alias_function_t)(struct AliasMenuData *wdata, int op);

/**
 * struct AliasFunction - A NeoMutt function
 */
struct AliasFunction
{
  int op;                    ///< Op code, e.g. OP_SEARCH
  alias_function_t function; ///< Function to call
};

extern struct AliasFunction AliasFunctions[];

void alias_array_sort(struct AliasViewArray *ava, const struct ConfigSubset *sub);
int alias_function_dispatcher(struct MuttWindow *win, int op);
bool alias_to_addrlist(struct AddressList *al, struct Alias *alias);
int query_run(const char *s, bool verbose, struct AliasList *al, const struct ConfigSubset *sub);


#endif /* MUTT_ALIAS_FUNCTIONS_H */
