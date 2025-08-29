/**
 * @file
 * Parse the Command Line
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
 * @page lib_cli Parse the Command Line
 *
 * | File             | Description             |
 * | :--------------- | :---------------------- |
 * | cli/parse.h      | @subpage cli_parse      |
 * | cli/objects.c    | @subpage cli_objects    |
 */

#ifndef MUTT_CLI_LIB_H
#define MUTT_CLI_LIB_H

// IWYU pragma: begin_keep
#include <stdbool.h>
#include "objects.h"
// IWYU pragma: end_keep

bool cli_parse(int argc, char **argv, struct CommandLine *cli);

void command_line_clear(struct CommandLine *cl);

#endif /* MUTT_CLI_LIB_H */
