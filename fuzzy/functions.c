/**
 * @file
 * Fuzzy functions
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page fuzzy_functions Fuzzy functions
 *
 * Fuzzy functions
 */

#include "config.h"
#include <string.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "fuzzy/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "module_data.h"

// clang-format off
/**
 * OpFuzzy - Functions for the Fuzzy Window
 */
static const struct MenuFuncOp OpFuzzy[] = { /* map: fuzzy */
  { "first-entry",                   OP_FIRST_ENTRY },
  { "last-entry",                    OP_LAST_ENTRY },
  { "next-entry",                    OP_NEXT_ENTRY },
  { "next-page",                     OP_NEXT_PAGE },
  { "previous-entry",                OP_PREV_ENTRY },
  { "previous-page",                 OP_PREV_PAGE },
  { NULL, 0 },
};

/**
 * FuzzyDefaultBindings - Key bindings for the Fuzzy Window
 */
const struct MenuOpSeq FuzzyDefaultBindings[] = {
  { OP_FIRST_ENTRY,                        "<home>" },
  { OP_LAST_ENTRY,                         "<end>" },
  { OP_NEXT_ENTRY,                         "<down>" },
  { OP_NEXT_PAGE,                          "<pagedown>" },
  { OP_PREV_ENTRY,                         "<up>" },
  { OP_PREV_PAGE,                          "<pageup>" },
  { 0, NULL },
};
// clang-format on

/**
 * fuzzy_init_keys - Initialise the Fuzzy Keybindings - Implements ::init_keys_api
 */
void fuzzy_init_keys(struct NeoMutt *n, struct SubMenu *sm_generic)
{
  struct FuzzyModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_FUZZY);
  ASSERT(mod_data);

  struct MenuDefinition *md = NULL;
  struct SubMenu *sm = NULL;

  sm = km_register_submenu(OpFuzzy);
  md = km_register_menu(MENU_FUZZY, "fuzzy");
  km_menu_add_submenu(md, sm);
  km_menu_add_bindings(md, FuzzyDefaultBindings);

  mod_data->md_fuzzy = md;
  mod_data->sm_fuzzy = sm;
}

/**
 * fuzzy_get_submenu - Get the Fuzzy SubMenu
 * @retval ptr Fuzzy SubMenu
 */
struct SubMenu *fuzzy_get_submenu(void)
{
  struct FuzzyModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_FUZZY);
  ASSERT(mod_data);

  return mod_data->sm_fuzzy;
}
