/**
 * @file
 * Set up the key bindings
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_KEY_INIT_H
#define MUTT_KEY_INIT_H

#include "keymap.h"
#include "menu.h"

struct NotifyCallback;

extern keycode_t AbortKey; ///< key to abort edits etc, normally Ctrl-G

extern struct SubMenuArray        SubMenus;
extern struct MenuDefinitionArray MenuDefs;

int                    km_config_observer  (struct NotifyCallback *nc);
void                   km_init              (void);
void                   km_menu_add_bindings    (struct MenuDefinition *md, const struct MenuOpSeq bindings[]);
void                   km_menu_add_submenu     (struct MenuDefinition *md, struct SubMenu *sm);
struct MenuDefinition *km_register_menu        (int menu, const char *name);
struct SubMenu *       km_register_submenu(const struct MenuFuncOp functions[]);
void                   km_set_abort_key  (void);
void                   km_cleanup    (void);

#endif /* MUTT_KEY_INIT_H */
