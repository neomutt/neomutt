/**
 * @file
 * Dump keybindings
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
 * @page debug_keymap Dump keybindings
 *
 * Dump keybindings
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "key/lib.h"

struct SubMenuId
{
  int id;
  struct SubMenu *sm;
};
ARRAY_HEAD(SubMenuIdArray, struct SubMenuId);

/**
 * log_bind - Log a key binding
 * @param md     Menu definition
 * @param keystr Key string
 * @param km     Key mapping
 */
void log_bind(const struct MenuDefinition *md, const char *keystr, struct Keymap *km)
{
  const char *fn_name = NULL;

  struct SubMenu **smp = NULL;
  ARRAY_FOREACH(smp, &md->submenus)
  {
    fn_name = help_lookup_function(md, km->op);
    if (fn_name)
      break;
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
 * dump_submenu_functions - Dump submenu functions
 * @param sm    Submenu to dump
 * @param brief True for brief output
 */
void dump_submenu_functions(const struct SubMenu *sm, bool brief)
{
  for (int i = 0; sm->functions[i].name; i++)
  {
    if (brief && (i > 2))
    {
      mutt_debug(LL_DEBUG1, "    ...\n");
      break;
    }

    const struct MenuFuncOp *mfo = &sm->functions[i];
    mutt_debug(LL_DEBUG1, "    \"%s\" -> %s (%d)\n", mfo->name,
               opcodes_get_name(mfo->op), mfo->op);
  }
}

/**
 * dump_submenu_bindings - Dump submenu bindings
 * @param md    Menu definition
 * @param sm    Submenu to dump
 * @param brief True for brief output
 */
void dump_submenu_bindings(const struct MenuDefinition *md, const struct SubMenu *sm, bool brief)
{
  if (STAILQ_EMPTY(&sm->keymaps))
  {
    mutt_debug(LL_DEBUG1, "    [NONE]\n");
    return;
  }

  struct Buffer *binding = buf_pool_get();
  struct Buffer *esc_key = buf_pool_get();

  int count = 0;
  struct Keymap *km = NULL;
  STAILQ_FOREACH(km, &sm->keymaps, entries)
  {
    if (brief && (count++ > 2))
    {
      mutt_debug(LL_DEBUG1, "    ...\n");
      break;
    }

    buf_reset(binding);
    keymap_expand_key(km, binding);

    buf_reset(esc_key);
    escape_string(esc_key, buf_string(binding));

    if (km->op == OP_MACRO)
      log_macro(buf_string(esc_key), km);
    else
      log_bind(md, buf_string(esc_key), km);
  }

  buf_pool_release(&binding);
  buf_pool_release(&esc_key);
}

/**
 * dump_submenus - Dump all submenus
 * @param brief True for brief output
 * @param smia  Submenu ID array to populate
 */
void dump_submenus(bool brief, struct SubMenuIdArray *smia)
{
  struct SubMenu *sm = NULL;
  ARRAY_FOREACH(sm, &SubMenus)
  {
    struct SubMenuId smi = { ARRAY_FOREACH_IDX_sm, sm };
    ARRAY_ADD(smia, smi);

    int i = 0;
    for (; sm->functions[i].name; i++)
      ; // Do nothing

    if (sm->parent)
      mutt_debug(LL_DEBUG1, "SubMenu ID %d (%d functions) -- %s:\n",
                 ARRAY_FOREACH_IDX_sm, i, sm->parent->name);
    else
      mutt_debug(LL_DEBUG1, "SubMenu ID %d (%d functions):\n", ARRAY_FOREACH_IDX_sm, i);

    dump_submenu_functions(sm, brief);
    mutt_debug(LL_DEBUG1, "\n");
  }
}

/**
 * dump_menus - Dump all menus
 * @param smia Submenu ID array
 */
void dump_menus(struct SubMenuIdArray *smia)
{
  struct Buffer *buf = buf_pool_get();
  struct MenuDefinition *md = NULL;

  mutt_debug(LL_DEBUG1, "Menus:\n");
  ARRAY_FOREACH(md, &MenuDefs)
  {
    buf_printf(buf, "    \"%s\" - %s (%d) - SubMenu IDs: ", md->name,
               name_menu_type(md->id), md->id);

    struct SubMenu **smp = NULL;
    ARRAY_FOREACH(smp, &md->submenus)
    {
      struct SubMenuId *smi = NULL;
      struct SubMenu *sm = *smp;
      int id = -1;

      ARRAY_FOREACH(smi, smia)
      {
        if (smi->sm == sm)
        {
          id = smi->id;
          break;
        }
      }

      const char *name = sm->parent ? sm->parent->name : "UNKNOWN";
      buf_add_printf(buf, "%s (%d)", name, id);

      if (ARRAY_FOREACH_IDX_smp < (ARRAY_SIZE(&md->submenus) - 1))
      {
        buf_add_printf(buf, ", ");
      }
    }
    mutt_debug(LL_DEBUG1, "%s\n", buf_string(buf));
  }

  buf_pool_release(&buf);
}

/**
 * dump_menu_funcs - Dump all menu functions
 * @param brief True for brief output
 */
void dump_menu_funcs(bool brief)
{
  struct SubMenuIdArray smia = ARRAY_HEAD_INITIALIZER;

  dump_submenus(brief, &smia);
  dump_menus(&smia);

  ARRAY_FREE(&smia);
}

/**
 * dump_menu_binds - Dump all menu bindings
 * @param brief True for brief output
 */
void dump_menu_binds(bool brief)
{
  struct MenuDefinition *md = NULL;
  ARRAY_FOREACH(md, &MenuDefs)
  {
    struct SubMenu *sm = *ARRAY_FIRST(&md->submenus);

    int count = 0;
    struct Keymap *km = NULL;
    STAILQ_FOREACH(km, &sm->keymaps, entries)
    {
      count++;
    }

    mutt_debug(LL_DEBUG1, "Menu %s (%s/%d) - (%d bindings):\n", md->name,
               name_menu_type(md->id), md->id, count);
    dump_submenu_bindings(md, sm, brief);
    mutt_debug(LL_DEBUG1, "\n");
  }
}
