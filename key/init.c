/**
 * @file
 * Set up the key bindings
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
static const struct Command KeyCommands[] = {
  // clang-format off
  { "bind",    parse_bind,   0 },
  { "exec",    parse_exec,   0 },
  { "macro",   parse_macro,  1 },
  { "push",    parse_push,   0 },
  { "unbind",  parse_unbind, MUTT_UNBIND },
  { "unmacro", parse_unbind, MUTT_UNMACRO },
  { NULL, NULL, 0 },
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

  commands_register(&NeoMutt->commands, KeyCommands);

  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, km_config_observer, NULL);
}

/**
 * km_cleanup - Free the key maps
 */
void km_cleanup(void)
{
  if (NeoMutt && NeoMutt->sub)
    notify_observer_remove(NeoMutt->sub->notify, km_config_observer, NULL);
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
