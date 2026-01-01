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
 * @param km     Keybinding
 */
void log_bind(enum MenuType menu, const char *keystr, struct Keymap *km)
{
  if (km->op == OP_NULL)
  {
    mutt_debug(LL_DEBUG1, "    bind %s noop\n", keystr);
    return;
  }

  const char *fn_name = NULL;
  /* The pager and editor menus don't use the generic km,
   * however for other menus try generic first. */
  if ((menu != MENU_PAGER) && (menu != MENU_EDITOR) && (menu != MENU_GENERIC))
  {
    fn_name = mutt_get_func(OpGeneric, km->op);
  }

  /* if it's one of the menus above or generic doesn't find the function,
   * try with its own menu. */
  if (!fn_name)
  {
    const struct MenuFuncOp *funcs = km_get_table(menu);
    if (!funcs)
      return;

    fn_name = mutt_get_func(funcs, km->op);
  }

  mutt_debug(LL_DEBUG1, "    bind %-8s <%s>\n", keystr, fn_name);
  mutt_debug(LL_DEBUG1, "        op = %d (%s)\n", km->op, opcodes_get_name(km->op));
  mutt_debug(LL_DEBUG1, "        eq = %d\n", km->eq);

  struct Buffer *keys = buf_pool_get();
  for (int i = 0; i < km->len; i++)
  {
    buf_add_printf(keys, "%d ", km->keys[i]);
  }
  mutt_debug(LL_DEBUG1, "        keys: %s\n", buf_string(keys));
  buf_pool_release(&keys);
}

/**
 * log_macro - Dumps all the macros maps of a menu into a buffer
 * @param keystr Bound string
 * @param km     Keybinding
 */
void log_macro(const char *keystr, struct Keymap *km)
{
  struct Buffer *esc_macro = buf_pool_get();
  escape_string(esc_macro, km->macro);

  mutt_debug(LL_DEBUG1, "    macro %-8s \"%s\"\n", keystr, buf_string(esc_macro));
  if (km->desc)
    mutt_debug(LL_DEBUG1, "        %s\n", km->desc);

  buf_pool_release(&esc_macro);

  mutt_debug(LL_DEBUG1, "        op = %d\n", km->op);
  mutt_debug(LL_DEBUG1, "        eq = %d\n", km->eq);
  struct Buffer *keys = buf_pool_get();
  for (int i = 0; i < km->len; i++)
  {
    buf_add_printf(keys, "%d ", km->keys[i]);
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
  struct Keymap *km = NULL;

  if (STAILQ_EMPTY(kml))
  {
    mutt_debug(LL_DEBUG1, "    [NONE]\n");
    return;
  }

  struct Buffer *binding = buf_pool_get();
  struct Buffer *esc_key = buf_pool_get();

  STAILQ_FOREACH(km, kml, entries)
  {
    buf_reset(binding);
    keymap_expand_key(km, binding);

    buf_reset(esc_key);
    escape_string(esc_key, buf_string(binding));

    if (km->op == OP_MACRO)
      log_macro(buf_string(esc_key), km);
    else
      log_bind(menu, buf_string(esc_key), km);
  }

  buf_pool_release(&binding);
  buf_pool_release(&esc_key);
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
