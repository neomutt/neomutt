/**
 * @file
 * Maniplate Menus and SubMenus
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page key_menu Maniplate Menus and SubMenus
 *
 * Maniplate Menus and SubMenus
 */

#include "config.h"
#include <string.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "menu.h"
#include "lib.h"
#include "init.h"
#include "keymap.h"

/**
 * km_find_func - Find a function's mapping in a Menu
 * @param mtype Menu type, e.g. #MENU_PAGER
 * @param func  Function, e.g. OP_DELETE
 * @retval ptr Keymap for the function
 */
struct Keymap *km_find_func(enum MenuType mtype, int func)
{
  struct Keymap *np = NULL;
  STAILQ_FOREACH(np, &Keymaps[mtype], entries)
  {
    if (np->op == func)
      break;
  }
  return np;
}

/**
 * km_get_op - Get the function by its name
 * @param funcs Functions table
 * @param start Name of function to find
 * @param len   Length of string to match
 * @retval num Operation, e.g. OP_DELETE
 */
int km_get_op(const struct MenuFuncOp *funcs, const char *start, size_t len)
{
  for (int i = 0; funcs[i].name; i++)
  {
    if (mutt_istrn_equal(start, funcs[i].name, len) && (mutt_str_len(funcs[i].name) == len))
    {
      return funcs[i].op;
    }
  }

  return OP_NULL;
}

/**
 * is_bound - Does a function have a keybinding?
 * @param km_list Keymap to examine
 * @param op      Operation, e.g. OP_DELETE
 * @retval true A key is bound to that operation
 */
bool is_bound(const struct KeymapList *km_list, int op)
{
  if (!km_list)
    return false;

  struct Keymap *map = NULL;
  STAILQ_FOREACH(map, km_list, entries)
  {
    if (map->op == op)
      return true;
  }

  return false;
}
