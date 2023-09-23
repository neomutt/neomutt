/**
 * @file
 * Dump keybindings
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
 * @page debug_keymap Dump keybindings
 *
 * Dump keybindings
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "key/lib.h"
#include "menu/lib.h"

/**
 * log_bind - Dumps all the binds maps of a menu into a buffer
 * @param menu   Menu to dump
 * @param keystr Bound string
 * @param map    Keybinding
 */
void log_bind(enum MenuType menu, const char *keystr, struct Keymap *map)
{
  if (map->op == OP_NULL)
  {
    mutt_debug(LL_DEBUG1, "    bind %s noop\n", keystr);
    return;
  }

  const char *fn_name = NULL;
  /* The pager and editor menus don't use the generic map,
   * however for other menus try generic first. */
  if ((menu != MENU_PAGER) && (menu != MENU_EDITOR) && (menu != MENU_GENERIC))
  {
    fn_name = mutt_get_func(OpGeneric, map->op);
  }

  /* if it's one of the menus above or generic doesn't find the function,
   * try with its own menu. */
  if (!fn_name)
  {
    const struct MenuFuncOp *funcs = km_get_table(menu);
    if (!funcs)
      return;

    fn_name = mutt_get_func(funcs, map->op);
  }

  mutt_debug(LL_DEBUG1, "    bind %-8s <%s>\n", keystr, fn_name);
  mutt_debug(LL_DEBUG1, "        op = %d (%s)\n", map->op, opcodes_get_name(map->op));
  mutt_debug(LL_DEBUG1, "        eq = %d\n", map->eq);

  struct Buffer *keys = buf_pool_get();
  for (int i = 0; i < map->len; i++)
  {
    buf_add_printf(keys, "%d ", map->keys[i]);
  }
  mutt_debug(LL_DEBUG1, "        keys: %s\n", buf_string(keys));
  buf_pool_release(&keys);
}

/**
 * log_macro - Dumps all the macros maps of a menu into a buffer
 * @param keystr Bound string
 * @param map    Keybinding
 */
void log_macro(const char *keystr, struct Keymap *map)
{
  struct Buffer *esc_macro = buf_pool_get();
  escape_string(esc_macro, map->macro);

  mutt_debug(LL_DEBUG1, "    macro %-8s \"%s\"\n", keystr, buf_string(esc_macro));
  if (map->desc)
    mutt_debug(LL_DEBUG1, "        %s\n", map->desc);

  buf_pool_release(&esc_macro);

  mutt_debug(LL_DEBUG1, "        op = %d\n", map->op);
  mutt_debug(LL_DEBUG1, "        eq = %d\n", map->eq);
  struct Buffer *keys = buf_pool_get();
  for (int i = 0; i < map->len; i++)
  {
    buf_add_printf(keys, "%d ", map->keys[i]);
  }
  mutt_debug(LL_DEBUG1, "        keys: %s\n", buf_string(keys));
  buf_pool_release(&keys);
}

/**
 * log_menu - Dump a Menu's keybindings to the log
 * @param menu Menu to dump
 * @param kml  Map of keybindings
 */
void log_menu(enum MenuType menu, struct KeymapList *kml)
{
  struct Keymap *map = NULL;

  if (STAILQ_EMPTY(kml))
  {
    mutt_debug(LL_DEBUG1, "    [NONE]\n");
    return;
  }

  STAILQ_FOREACH(map, kml, entries)
  {
    char key_binding[128] = { 0 };
    km_expand_key(key_binding, sizeof(key_binding), map);

    struct Buffer *esc_key = buf_pool_get();
    escape_string(esc_key, key_binding);

    if (map->op == OP_MACRO)
      log_macro(buf_string(esc_key), map);
    else
      log_bind(menu, buf_string(esc_key), map);

    buf_pool_release(&esc_key);
  }
}

/**
 * dump_keybindings - Dump all the keybindings to the log
 */
void dump_keybindings(void)
{
  mutt_debug(LL_DEBUG1, "Keybindings:\n");
  for (int i = 1; i < MENU_MAX; i++)
  {
    mutt_debug(LL_DEBUG1, "Menu: %s\n", mutt_map_get_name(i, MenuNames));
    log_menu(i, &Keymaps[i]);
  }
}
