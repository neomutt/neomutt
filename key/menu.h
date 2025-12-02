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
#include "core/lib.h"
#include "menu/lib.h"
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

bool                   is_bound           (const struct KeymapList *km_list, int op);
struct Keymap *        km_find_func       (enum MenuType mtype, int func);
int                    km_get_op          (const struct MenuFuncOp *funcs, const char *start, size_t len);

#endif /* MUTT_KEY_MENU_H */
