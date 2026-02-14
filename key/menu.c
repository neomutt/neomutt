/**
 * @file
 * Maniplate Menus and SubMenus
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
#include "get.h"
#include "init.h"
#include "keymap.h"

/**
 * km_bind - Set up a key binding
 * @param md      Menu Definition
 * @param key_str Key string
 * @param op      Operation, e.g. OP_DELETE
 * @param macro   Macro string
 * @param desc    Description of macro (OPTIONAL)
 * @param err     Buffer for error message
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * Insert a key sequence into the specified map.
 * The map is sorted by ASCII value (lowest to highest)
 */
enum CommandResult km_bind(struct MenuDefinition *md, const char *key_str,
                           int op, char *macro, char *desc, struct Buffer *err)
{
  if (!md)
    return MUTT_CMD_ERROR;
  (void) err;

  enum CommandResult rc = MUTT_CMD_SUCCESS;
  struct Keymap *last = NULL;
  struct Keymap *np = NULL;
  struct Keymap *compare = NULL;
  keycode_t buf[KEY_SEQ_MAX_LEN] = { 0 };
  size_t pos = 0;
  size_t lastpos = 0;

  struct SubMenu *sm = *ARRAY_FIRST(&md->submenus);
  struct KeymapList *kml = &sm->keymaps;

  size_t len = parse_keys(key_str, buf, KEY_SEQ_MAX_LEN);

  struct Keymap *map = keymap_alloc(len, buf);
  map->op = op;
  map->macro = mutt_str_dup(macro);
  map->desc = mutt_str_dup(desc);

  /* find position to place new keymap */
  STAILQ_FOREACH(np, kml, entries)
  {
    compare = keymap_compare(map, np, &pos);

    if (compare == map) /* map's keycode is bigger */
    {
      last = np;
      lastpos = pos;
      if (pos > np->eq)
        pos = np->eq;
    }
    else if (compare == np) /* np's keycode is bigger, found insert location */
    {
      map->eq = pos;
      break;
    }
    else /* equal keycodes */
    {
      if (np->len < len)
      {
        // Prefix-compatible binding, continue looking for insertion point.
        last = np;
        lastpos = np->len;
        if (pos > np->eq)
          pos = np->eq;
        continue;
      }
      else if (np->len > len)
      {
        // Prefix-compatible binding, insert before the longer sequence.
        map->eq = len;
        break;
      }

      // Exact same key sequence: replace existing mapping.
      map->eq = np->eq;
      STAILQ_REMOVE(kml, np, Keymap, entries);
      keymap_free(&np);
      break;
    }
  }

  if (map->op == OP_NULL)
  {
    keymap_free(&map);
  }
  else
  {
    if (last) /* if queue has at least one entry */
    {
      if (STAILQ_NEXT(last, entries))
        STAILQ_INSERT_AFTER(kml, last, map, entries);
      else /* last entry in the queue */
        STAILQ_INSERT_TAIL(kml, map, entries);
      last->eq = lastpos;
    }
    else /* queue is empty, so insert from head */
    {
      STAILQ_INSERT_HEAD(kml, map, entries);
    }
  }

  return rc;
}

/**
 * km_find_func - Find a function's mapping in a Menu
 * @param mtype Menu type, e.g. #MENU_PAGER
 * @param func  Function, e.g. OP_DELETE
 * @retval ptr Keymap for the function
 */
struct Keymap *km_find_func(enum MenuType mtype, int func)
{
  struct MenuDefinition *md = NULL;
  ARRAY_FOREACH(md, &MenuDefs)
  {
    if (md->id == mtype)
      break;
  }

  struct SubMenu **smp = NULL;

  ARRAY_FOREACH(smp, &md->submenus)
  {
    struct SubMenu *sm = *smp;

    struct Keymap *map = NULL;
    STAILQ_FOREACH(map, &sm->keymaps, entries)
    {
      if (map->op == func)
        return map;
    }
  }

  return NULL;
}

/**
 * km_get_menu_name - Get the name of a Menu
 * @param mtype Menu Type
 * @retval str Menu name, e.g. "index"
 */
const char *km_get_menu_name(int mtype)
{
  struct MenuDefinition *md = NULL;
  ARRAY_FOREACH(md, &MenuDefs)
  {
    if (md->id == mtype)
      return md->name;
  }

  return "UNKNOWN";
}

/**
 * km_get_menu_id - Get the ID of a Menu
 * @param name Menu name, e.g. "index"
 * @retval num Menu ID, e.g. #MENU_INDEX
 */
int km_get_menu_id(const char *name)
{
  struct MenuDefinition *md = NULL;
  ARRAY_FOREACH(md, &MenuDefs)
  {
    if (mutt_str_equal(md->name, name))
      return md->id;
  }

  return -1;
}

/**
 * km_get_op - Get the OpCode for a Function
 * @param func Function name, e.g. "exit"
 * @retval num OpCode, e.g. OP_EXIT
 */
int km_get_op(const char *func)
{
  struct MenuDefinition *md = NULL;
  ARRAY_FOREACH(md, &MenuDefs)
  {
    struct SubMenu **smp = NULL;

    ARRAY_FOREACH(smp, &md->submenus)
    {
      struct SubMenu *sm = *smp;

      for (int i = 0; sm->functions[i].name; i++)
      {
        if (mutt_str_equal(sm->functions[i].name, func))
          return sm->functions[i].op;
      }
    }
  }

  return OP_NULL;
}

/**
 * km_get_op_menu - Get the OpCode for a Function from a Menu
 * @param mtype Menu Type, e.g. #MENU_INDEX
 * @param func  Function name, e.g. "exit"
 * @retval num OpCode, e.g. OP_EXIT
 */
int km_get_op_menu(int mtype, const char *func)
{
  struct MenuDefinition *md = NULL;
  ARRAY_FOREACH(md, &MenuDefs)
  {
    if (md->id != mtype)
      continue;

    struct SubMenu **smp = NULL;

    ARRAY_FOREACH(smp, &md->submenus)
    {
      struct SubMenu *sm = *smp;

      for (int i = 0; sm->functions[i].name; i++)
      {
        if (mutt_str_equal(sm->functions[i].name, func))
          return sm->functions[i].op;
      }
    }
  }

  return OP_NULL;
}

/**
 * menu_find - Find a Menu Definition by Menu type
 * @param menu Menu Type, e.g. #MENU_INDEX
 * @retval ptr Menu Definition
 */
struct MenuDefinition *menu_find(int menu)
{
  struct MenuDefinition *md = NULL;
  ARRAY_FOREACH(md, &MenuDefs)
  {
    if (md->id == menu)
      return md;
  }

  return NULL;
}

/**
 * is_bound - Does a function have a keybinding?
 * @param md Menu Definition
 * @param op Operation, e.g. OP_DELETE
 * @retval true A key is bound to that operation
 */
bool is_bound(const struct MenuDefinition *md, int op)
{
  struct SubMenu **smp = NULL;

  ARRAY_FOREACH(smp, &md->submenus)
  {
    struct SubMenu *sm = *smp;

    struct Keymap *map = NULL;
    STAILQ_FOREACH(map, &sm->keymaps, entries)
    {
      if (map->op == op)
        return true;
    }
  }

  return false;
}
