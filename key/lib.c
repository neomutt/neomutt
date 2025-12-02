/**
 * @file
 * Key helper functions
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page key_lib Key helper functions
 *
 * Key helper functions
 */

#include "config.h"
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "menu/lib.h"

extern const struct MenuFuncOp OpAlias[];
extern const struct MenuFuncOp OpAttachment[];
#ifdef USE_AUTOCRYPT
extern const struct MenuFuncOp OpAutocrypt[];
#endif
extern const struct MenuFuncOp OpBrowser[];
extern const struct MenuFuncOp OpCompose[];
extern const struct MenuFuncOp OpEditor[];
extern const struct MenuFuncOp OpIndex[];
extern const struct MenuFuncOp OpPager[];
extern const struct MenuFuncOp OpPgp[];
extern const struct MenuFuncOp OpPostponed[];
extern const struct MenuFuncOp OpQuery[];
extern const struct MenuFuncOp OpSmime[];

/// Array of key mappings, one for each #MenuType
struct KeymapList Keymaps[MENU_MAX];

/**
 * mutt_get_func - Get the name of a function
 * @param funcs Functions table
 * @param op    Operation, e.g. OP_DELETE
 * @retval ptr  Name of function
 * @retval NULL Operation not found
 *
 * @note This returns a static string.
 */
const char *mutt_get_func(const struct MenuFuncOp *funcs, int op)
{
  if (!funcs)
    return NULL;

  for (int i = 0; funcs[i].name; i++)
  {
    if (funcs[i].op == op)
      return funcs[i].name;
  }

  return NULL;
}

/**
 * km_get_table - Lookup a Menu's functions
 * @param mtype Menu type, e.g. #MENU_EDITOR
 * @retval ptr Array of functions
 */
const struct MenuFuncOp *km_get_table(enum MenuType mtype)
{
  switch (mtype)
  {
    case MENU_ALIAS:
      return OpAlias;
    case MENU_ATTACHMENT:
      return OpAttachment;
#ifdef USE_AUTOCRYPT
    case MENU_AUTOCRYPT:
      return OpAutocrypt;
#endif
    case MENU_BROWSER:
      return OpBrowser;
    case MENU_COMPOSE:
      return OpCompose;
    case MENU_DIALOG:
      return OpDialog;
    case MENU_EDITOR:
      return OpEditor;
    case MENU_GENERIC:
      return OpGeneric;
    case MENU_INDEX:
      return OpIndex;
    case MENU_PAGER:
      return OpPager;
    case MENU_PGP:
      return OpPgp;
    case MENU_POSTPONED:
      return OpPostponed;
    case MENU_QUERY:
      return OpQuery;
    case MENU_SMIME:
      return OpSmime;
    default:
      return NULL;
  }
}
