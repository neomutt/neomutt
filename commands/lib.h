/**
 * @file
 * NeoMutt Commands
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
 * @page lib_commands NeoMutt Commands
 *
 * NeoMutt Commands
 *
 * | File                   | Description                   |
 * | :--------------------- | :---------------------------- |
 * | commands/commands.c    | @subpage commands_commands    |
 * | commands/ifdef.c       | @subpage commands_ifdef       |
 * | commands/mailboxes.c   | @subpage commands_mailboxes   |
 * | commands/module.c      | @subpage commands_module      |
 * | commands/parse.c       | @subpage commands_parse       |
 * | commands/setenv.c      | @subpage commands_setenv      |
 * | commands/source.c      | @subpage commands_source      |
 * | commands/stailq.c      | @subpage commands_stailq      |
 */

#ifndef MUTT_COMMANDS_LIB_H
#define MUTT_COMMANDS_LIB_H

// IWYU pragma: begin_keep
#include "commands.h"
#include "ifdef.h"
#include "mailboxes.h"
#include "parse.h"
#include "setenv.h"
#include "source.h"
#include "stailq.h"
// IWYU pragma: end_keep

#endif /* MUTT_COMMANDS_LIB_H */
