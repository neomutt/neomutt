/**
 * @file
 * Dump key bindings
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

#ifndef MUTT_KEY_DUMP_H
#define MUTT_KEY_DUMP_H

#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "menu/lib.h"

struct KeymapList;
struct MenuFuncOp;

/**
 * struct BindingInfo - Info about one keybinding
 *
 * - `bind`:  [key, function,   description]
 * - `macro`: [key, macro-text, description]
 */
struct BindingInfo
{
  const char *a[3]; ///< Array of info
};
ARRAY_HEAD(BindingInfoArray, struct BindingInfo);

int                binding_sort        (const void *a, const void *b, void *sdata);
void               colon_bind          (enum MenuType menu, FILE *fp);
void               colon_macro         (enum MenuType menu, FILE *fp);
void               gather_menu         (enum MenuType menu, struct BindingInfoArray *bia_bind, struct BindingInfoArray *bia_macro);
enum CommandResult parse_bind_macro    (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
int                gather_unbound      (const struct MenuFuncOp *funcs, const struct KeymapList *km_menu, const struct KeymapList *km_aux, struct BindingInfoArray *bia_unbound);
int                measure_column      (struct BindingInfoArray *bia, int col);
int                print_bind          (enum MenuType menu, FILE *fp);
int                print_macro         (enum MenuType menu, FILE *fp);

#endif /* MUTT_KEY_DUMP_H */
