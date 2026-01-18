/**
 * @file
 * Set up the key bindings
 *
 * @authors
 * Copyright (C) 2023-2026 Richard Russon <rich@flatcap.org>
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
 * @page key_init Set up the key bindings
 *
 * Set up the key bindings
 */

#include "config.h"
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "init.h"
#include "commands.h"
#include "get.h"
#include "keymap.h"
#include "menu.h"

/// All the registered Menus
struct MenuDefinitionArray MenuDefs;

/// All the registered SubMenus
struct SubMenuArray SubMenus;

keycode_t AbortKey; ///< code of key to abort prompts, normally Ctrl-G

/**
 * KeyCommands - Key Binding Commands
 */
const struct Command KeyCommands[] = {
  // clang-format off
  { "bind", CMD_BIND, parse_bind, CMD_NO_DATA,
        N_("Bind a key to a function"),
        N_("bind <map>[,<map> ... ] <key> <function>"),
        "configuration.html#bind" },
  { "exec", CMD_EXEC, parse_exec, CMD_NO_DATA,
        N_("Execute a function"),
        N_("exec <function> [ <function> ... ]"),
        "configuration.html#exec" },
  { "macro", CMD_MACRO, parse_macro, CMD_NO_DATA,
        N_("Define a keyboard macro"),
        N_("macro <map>[,<map> ... ] <key> <sequence> [ <description> ]"),
        "configuration.html#macro" },
  { "push", CMD_PUSH, parse_push, CMD_NO_DATA,
        N_("Push a string into NeoMutt's input queue (simulate typing)"),
        N_("push <string>"),
        "configuration.html#push" },
  { "unbind", CMD_UNBIND, parse_unbind, CMD_NO_DATA,
        N_("Remove a key binding"),
        N_("unbind { * | <map>[,<map> ... ] } [ <key> ]"),
        "configuration.html#unbind" },
  { "unmacro", CMD_UNMACRO, parse_unbind, CMD_NO_DATA,
        N_("Remove a keyboard `macro`"),
        N_("unmacro { * | <map>[,<map> ... ] } [ <key> ]"),
        "configuration.html#unmacro" },

  { NULL, CMD_NONE, NULL, CMD_NO_DATA, NULL, NULL, NULL, CF_NO_FLAGS },
  // clang-format on
};

/**
 * km_register_submenu - Register a submenu
 * @param functions Function definitions
 * @retval ptr SubMenu
 *
 * Register a set of functions.
 * The result can be used in multiple Menus.
 */
struct SubMenu *km_register_submenu(const struct MenuFuncOp functions[])
{
  struct SubMenu sm = { 0 };
  sm.functions = functions;
  ARRAY_INIT(&sm.keymaps);

  ARRAY_ADD(&SubMenus, sm);
  return ARRAY_LAST(&SubMenus);
}

/**
 * km_register_menu - Register a menu
 * @param menu Menu Type, e.g. #MENU_INDEX
 * @param name Menu name, e.g. "index"
 * @retval ptr Menu Definition
 */
struct MenuDefinition *km_register_menu(int menu, const char *name)
{
  struct MenuDefinition md = { 0 };
  md.id = menu;
  md.name = mutt_str_dup(name);
  ARRAY_INIT(&md.submenus);

  ARRAY_ADD(&MenuDefs, md);
  return ARRAY_LAST(&MenuDefs);
}

/**
 * km_menu_add_submenu - Add a SubMenu to a Menu Definition
 * @param md Menu Definition
 * @param sm SubMenu to add
 */
void km_menu_add_submenu(struct MenuDefinition *md, struct SubMenu *sm)
{
  if (!sm->parent)
    sm->parent = md;

  ARRAY_ADD(&md->submenus, sm);
}

/**
 * km_menu_add_bindings - Add Keybindings to a Menu
 * @param md       Menu Definition
 * @param bindings Keybindings to add
 */
void km_menu_add_bindings(struct MenuDefinition *md, const struct MenuOpSeq bindings[])
{
  for (int i = 0; bindings[i].op != OP_NULL; i++)
  {
    if (bindings[i].seq)
    {
      km_bind(md, bindings[i].seq, bindings[i].op, NULL, NULL, NULL);
    }
  }
}

/**
 * km_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
int km_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;

  if (!mutt_str_equal(ev_c->name, "abort_key"))
    return 0;

  km_set_abort_key();
  mutt_debug(LL_DEBUG5, "config done\n");
  return 0;
}

/**
 * km_init - Initialise all the menu keybindings
 */
void km_init(void)
{
  ARRAY_INIT(&MenuDefs);
  ARRAY_INIT(&SubMenus);

  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, km_config_observer, NULL);
}

/**
 * km_cleanup - Free the key maps
 */
void km_cleanup(void)
{
  if (NeoMutt && NeoMutt->sub)
    notify_observer_remove(NeoMutt->sub->notify, km_config_observer, NULL);

  struct MenuDefinition *md = NULL;
  ARRAY_FOREACH(md, &MenuDefs)
  {
    FREE(&md->name);
    ARRAY_FREE(&md->submenus);
  }
  ARRAY_FREE(&MenuDefs);

  struct SubMenu *sm = NULL;
  ARRAY_FOREACH(sm, &SubMenus)
  {
    keymaplist_free(&sm->keymaps);
  }
  ARRAY_FREE(&SubMenus);

  ARRAY_FREE(&MacroEvents);
  ARRAY_FREE(&UngetKeyEvents);
}

/**
 * km_set_abort_key - Parse the abort_key config string
 *
 * Parse the string into `$abort_key` and put the keycode into AbortKey.
 */
void km_set_abort_key(void)
{
  keycode_t buf[4] = { 0 };
  const char *const c_abort_key = cs_subset_string(NeoMutt->sub, "abort_key");

  size_t len = parse_keys(c_abort_key, buf, countof(buf));
  if (len == 0)
  {
    mutt_error(_("Abort key is not set, defaulting to Ctrl-G"));
    AbortKey = ctrl('G');
    return;
  }

  if (len > 1)
  {
    mutt_warning(_("Specified abort key sequence (%s) will be truncated to first key"),
                 c_abort_key);
  }
  AbortKey = buf[0];
}
