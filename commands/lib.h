/**
 * @file
 * NeoMutt Commands
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
 * @page lib_command NeoMutt Commands
 *
 * NeoMutt Commands
 *
 * | File                   | Description                   |
 * | :--------------------- | :---------------------------- |
 * | commands/alternates.c  | @subpage commands_alternates  |
 * | commands/commands.c    | @subpage commands_commands    |
 * | commands/group.c       | @subpage commands_group       |
 * | commands/ifdef.c       | @subpage commands_ifdef       |
 * | commands/ignore.c      | @subpage commands_ignore      |
 * | commands/mailboxes.c   | @subpage commands_mailboxes   |
 * | commands/module.c      | @subpage commands_module      |
 * | commands/my_header.c   | @subpage commands_my_header   |
 * | commands/parse.c       | @subpage commands_parse       |
 * | commands/setenv.c      | @subpage commands_setenv      |
 * | commands/source.c      | @subpage commands_source      |
 * | commands/spam.c        | @subpage commands_spam        |
 * | commands/stailq.c      | @subpage commands_stailq      |
 * | commands/subjectrx.c   | @subpage commands_subjectrx   |
 * | commands/tags.c        | @subpage commands_tags        |
 */

#ifndef MUTT_COMMANDS_LIB_H
#define MUTT_COMMANDS_LIB_H

// IWYU pragma: begin_keep
#include "alternates.h"
#include "commands.h"
#include "group.h"
#include "ifdef.h"
#include "ignore.h"
#include "mailboxes.h"
#include "my_header.h"
#include "parse.h"
#include "setenv.h"
#include "source.h"
#include "spam.h"
#include "stailq.h"
#include "subjectrx.h"
#include "tags.h"
// IWYU pragma: end_keep

#endif /* MUTT_COMMANDS_LIB_H */
