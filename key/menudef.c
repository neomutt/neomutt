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

/**
 * @page key_menudef Menu Definitions
 *
 * Menu Definitions
 */

#include "config.h"
#include "mutt/lib.h"
#include "menudef.h"
#include "keymap.h"

/**
 * submenu_free - Free a SubMenu
 * @param pptr SubMenu to free
 */
void submenu_free(struct SubMenu **pptr)
{
  if (!pptr || !*pptr)
    return;

  struct SubMenu *sm = *pptr;
  keymaplist_free(&sm->keymaps);

  FREE(pptr);
}

/**
 * submenu_new - Create a new SubMenu
 * @retval New SubMenu
 */
struct SubMenu *submenu_new(void)
{
  struct SubMenu *sm = MUTT_MEM_CALLOC(1, struct SubMenu);

  ARRAY_INIT(&sm->keymaps);

  return sm;
}

/**
 * menudef_free - Free a MenuDefinition
 * @param pptr MenuDefinition to free
 */
void menudef_free(struct MenuDefinition **pptr)
{
  if (!pptr || !*pptr)
    return;

  struct MenuDefinition *md = *pptr;

  FREE(&md->name);

  ARRAY_FREE(&md->submenus);

  FREE(pptr);
}

/**
 * menudef_new - Create a new MenuDefinition
 * @retval ptr New MenuDefinition
 */
struct MenuDefinition *menudef_new(void)
{
  struct MenuDefinition *md = MUTT_MEM_CALLOC(1, struct MenuDefinition);

  ARRAY_INIT(&md->submenus);

  return md;
}
