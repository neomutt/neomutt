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

/**
 * @page alias_functions Alias functions
 *
 * Alias functions
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "functions.h"
#include "lib.h"
#include "index/lib.h"
#include "opcodes.h"

/**
 * op_create_alias - create an alias from a message sender - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_create_alias(struct AliasMenuData *mdata, int op)
{
  return IR_NOT_IMPL;
}

/**
 * op_delete - delete the current entry - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_delete(struct AliasMenuData *mdata, int op)
{
  return IR_NOT_IMPL;
}

/**
 * op_exit - exit this menu - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_exit(struct AliasMenuData *mdata, int op)
{
  return IR_NOT_IMPL;
}

/**
 * op_generic_select_entry - select the current entry - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_generic_select_entry(struct AliasMenuData *mdata, int op)
{
  return IR_NOT_IMPL;
}

/**
 * op_main_limit - show only messages matching a pattern - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_main_limit(struct AliasMenuData *mdata, int op)
{
  return IR_NOT_IMPL;
}

/**
 * op_query - query external program for addresses - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_query(struct AliasMenuData *mdata, int op)
{
  return IR_NOT_IMPL;
}

/**
 * op_search - search for a regular expression - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_search(struct AliasMenuData *mdata, int op)
{
  return IR_NOT_IMPL;
}

/**
 * op_sort - sort aliases - Implements ::alias_function_t - @ingroup alias_function_api
 */
static int op_sort(struct AliasMenuData *mdata, int op)
{
  return IR_NOT_IMPL;
}

// -----------------------------------------------------------------------------

/**
 * AliasFunctions - All the NeoMutt functions that the Alias supports
 */
struct AliasFunction AliasFunctions[] = {
  // clang-format off
  { OP_CREATE_ALIAS,           op_create_alias },
  { OP_DELETE,                 op_delete },
  { OP_EXIT,                   op_exit },
  { OP_GENERIC_SELECT_ENTRY,   op_generic_select_entry },
  { OP_MAIL,                   op_generic_select_entry },
  { OP_MAIN_LIMIT,             op_main_limit },
  { OP_QUERY,                  op_query },
  { OP_QUERY_APPEND,           op_query },
  { OP_SEARCH,                 op_search },
  { OP_SEARCH_NEXT,            op_search },
  { OP_SEARCH_OPPOSITE,        op_search },
  { OP_SEARCH_REVERSE,         op_search },
  { OP_SORT,                   op_sort },
  { OP_SORT_REVERSE,           op_sort },
  { OP_UNDELETE,               op_delete },
  { 0, NULL },
  // clang-format on
};

/**
 * alias_function_dispatcher - Perform a Alias function
 * @param win Alias Window
 * @param op  Operation to perform, e.g. OP_ALIAS_NEXT
 * @retval num #IndexRetval, e.g. #IR_SUCCESS
 */
int alias_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return IR_UNKNOWN;

  struct Menu *menu = win->wdata;
  struct AliasMenuData *mdata = menu->mdata;
  int rc = IR_UNKNOWN;
  for (size_t i = 0; AliasFunctions[i].op != OP_NULL; i++)
  {
    const struct AliasFunction *fn = &AliasFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(mdata, op);
      break;
    }
  }

  if (rc == IR_UNKNOWN) // Not our function
    return rc;

  const char *result = mutt_map_get_name(rc, RetvalNames);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", OpStrings[op][0], op, NONULL(result));

  return rc;
}
