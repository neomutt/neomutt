/**
 * @file
 * Menu Definitions
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

#ifndef MUTT_KEY_MENUDEF_H
#define MUTT_KEY_MENUDEF_H

#include "mutt/lib.h"
#include "keymap.h"

/**
 * struct SubMenu - Collection of related functions
 */
struct SubMenu
{
  struct MenuDefinition   *parent;      ///< Primary parent
  const struct MenuFuncOp *functions;   ///< All available functions
  struct KeymapList        keymaps;     ///< All keybindings
};
ARRAY_HEAD(SubMenuArray, struct SubMenu *);

/**
 * struct MenuDefinition - Functions for a Dialog or Window
 */
struct MenuDefinition
{
  int                  id;         ///< Menu ID, e.g. #MENU_ALIAS
  const char          *name;       ///< Menu name, e.g. "alias"
  struct SubMenuArray  submenus;   ///< Parts making up the Menu
};
ARRAY_HEAD(MenuDefinitionArray, struct MenuDefinition *);

void            submenu_free(struct SubMenu **pptr);
struct SubMenu *submenu_new (void);

void                   menudef_free(struct MenuDefinition **pptr);
struct MenuDefinition *menudef_new (void);

#endif /* MUTT_KEY_MENUDEF_H */
