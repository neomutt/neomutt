/**
 * @file
 * Hook Commands
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

/**
 * @page lib_hooks Hook Commands
 *
 * Hook Commands
 *
 * | File               | Description              |
 * | :----------------- | :----------------------- |
 * | hooks/commands.c   | @subpage hooks_commands  |
 * | hooks/config.c     | @subpage hooks_config    |
 * | hooks/dump.c       | @subpage hooks_dump      |
 * | hooks/exec.c       | @subpage hooks_exec      |
 * | hooks/hook.c       | @subpage hooks_hook      |
 * | hooks/module.c     | @subpage hooks_module    |
 * | hooks/parse.c      | @subpage hooks_parse     |
 */

#ifndef MUTT_HOOKS_LIB_H
#define MUTT_HOOKS_LIB_H

// IWYU pragma: begin_keep
#include "commands.h"
#include "dump.h"
#include "exec.h"
#include "hook.h"
#include "parse.h"
// IWYU pragma: end_keep

#endif /* MUTT_HOOKS_LIB_H */
