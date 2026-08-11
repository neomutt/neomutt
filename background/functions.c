/**
 * @file
 * Background commands - custom functions
 *
 * @authors
 * Copyright (C) 2026 Reza Jelveh
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
 * @page background_functions Background functions
 *
 * Background functions
 */

#include "config.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "key/lib.h"
#include "menu/lib.h"

// clang-format off
/**
 * OpBackground - Functions for the Background Commands Dialog
 */
static const struct MenuFuncOp OpBackground[] = { /* map: background */
  { "background-clear",              OP_BACKGROUND_CLEAR },
  { NULL, 0 },
};

/**
 * BackgroundDefaultBindings - Key bindings for the Background Commands Dialog
 */
static const struct MenuOpSeq BackgroundDefaultBindings[] = { /* map: background */
  { OP_BACKGROUND_CLEAR,                    "x" },
  { OP_DELETE,                              "d" },
  { OP_SAVE,                                "s" },
  { 0, NULL },
};
// clang-format on

/**
 * background_init_keys - Initialise the Background Keybindings - Implements ::init_keys_api
 */
void background_init_keys(struct NeoMutt *n, struct SubMenu *sm_generic)
{
  struct MenuDefinition *md = NULL;
  struct SubMenu *sm = NULL;

  sm = km_register_submenu(OpBackground);
  md = km_register_menu(MENU_BACKGROUND, "background");
  km_menu_add_submenu(md, sm);
  km_menu_add_submenu(md, sm_generic);
  km_menu_add_bindings(md, BackgroundDefaultBindings);
}
