/**
 * @file
 * Maniplate Menus and SubMenus
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

#ifndef MUTT_KEY_MENU_H
#define MUTT_KEY_MENU_H

#include <stdbool.h>
#include "mutt/lib.h"
#include "menu/lib.h"
#include "get.h"
#include "keymap.h"

/**
 * struct MenuFuncOp - Mapping between a function and an operation
 */
struct MenuFuncOp
{
  const char   *name;    ///< Name of the function
  int           op;      ///< Operation, e.g. OP_DELETE
  MenuFuncFlags flags;   ///< Flags, e.g. MFF_DEPRECATED
};

/**
 * struct MenuOpSeq - Mapping between an operation and a key sequence
 */
struct MenuOpSeq
{
  int op;           ///< Operation, e.g. OP_DELETE
  const char *seq;  ///< Default key binding
};

/**
 * struct MenuFunctionOp - Mapping between a function and an operation
 */
struct MenuFunctionOp
{
  int         menu;       ///< Menu, e.g. #MENU_ALIAS
  const char *function;   ///< Name of the function
  int         op;         ///< Operation, e.g. OP_DELETE
};
ARRAY_HEAD(MenuFunctionOpArray, struct MenuFunctionOp);

/**
 * struct SubMenu - Collection of related functions
 */
struct SubMenu
{
  struct MenuDefinition   *parent;      ///< Primary parent
  const struct MenuFuncOp *functions;   ///< All available functions
  struct KeymapList        keymaps;     ///< All keybindings
};
ARRAY_HEAD(SubMenuArray,  struct SubMenu);
ARRAY_HEAD(SubMenuPArray, struct SubMenu *);

/**
 * struct MenuDefinition - Functions for a Dialog or Window
 */
struct MenuDefinition
{
  int                   id;         ///< Menu ID, e.g. #MENU_ALIAS
  const char           *name;       ///< Menu name, e.g. "alias"
  struct SubMenuPArray  submenus;   ///< Parts making up the Menu
};
ARRAY_HEAD(MenuDefinitionArray, struct MenuDefinition);

/**
 * @defgroup init_keys_api Initialise Key Bindings
 *
 * init_keys_t - Initialise Key Bindings
 * @param[in] sm_generic Generic SubMenu
 *
 * Register menus and submenus.
 */
typedef void (*init_keys_t)(struct SubMenu *sm_generic);

bool                   is_bound           (const struct MenuDefinition *md, int op);
struct Keymap *        km_find_func       (enum MenuType menu, int func);
int                    km_get_menu_id     (const char *name);
const char *           km_get_menu_name   (int mtype);
int                    km_get_op          (const char *func);
int                    km_get_op_menu     (int mtype, const char *func);
struct MenuDefinition *menu_find          (int menu);

#endif /* MUTT_KEY_MENU_H */
