/**
 * @file
 * Manage keymappings
 *
 * @authors
 * Copyright (C) 2017-2025 Richard Russon <rich@flatcap.org>
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
 * @page lib_key Key mappings
 *
 * Manage keymappings
 *
 * | File                | Description           |
 * | :------------------ | :-------------------- |
 * | key/commands.c      | @subpage key_commands |
 * | key/dump.c          | @subpage key_dump     |
 * | key/extended.c      | @subpage key_extended |
 * | key/get.c           | @subpage key_get      |
 * | key/init.c          | @subpage key_init     |
 * | key/keymap.c        | @subpage key_keymap   |
 * | key/menu.c          | @subpage key_menu     |
 * | key/module.c        | @subpage key_module   |
 */

#ifndef MUTT_KEY_LIB_H
#define MUTT_KEY_LIB_H

// IWYU pragma: begin_keep
#include "commands.h"
#include "dump.h"
#include "extended.h"
#include "get.h"
#include "init.h"
#include "keymap.h"
#include "menu.h"
#include "notify.h"
// IWYU pragma: end_keep

#endif /* MUTT_KEY_LIB_H */
