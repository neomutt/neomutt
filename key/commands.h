/**
 * @file
 * Parse key binding commands
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

#ifndef MUTT_KEY_COMMANDS_H
#define MUTT_KEY_COMMANDS_H

#include <stdbool.h>
#include <stdint.h>
#include "core/lib.h"
#include "menu/lib.h"

struct Buffer;

#define MUTT_UNBIND   (1 << 0)  ///< Parse 'unbind'  command
#define MUTT_UNMACRO  (1 << 1)  ///< Parse 'unmacro' command

enum CommandResult     km_bind            (const char *s, enum MenuType mtype, int op, char *macro, char *desc, struct Buffer *err);
enum CommandResult     parse_bind         (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult     parse_exec         (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
char *                 parse_keymap       (enum MenuType *mtypes, struct Buffer *s, int max_menus, int *num_menus, struct Buffer *err, bool bind);
enum CommandResult     parse_macro        (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
void *                 parse_menu         (bool *menus, const char *s, struct Buffer *err);
enum CommandResult     parse_push         (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult     parse_unbind       (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);

#endif /* MUTT_KEY_COMMANDS_H */
