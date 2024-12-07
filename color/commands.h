/**
 * @file
 * Parse colour commands
 *
 * @authors
 * Copyright (C) 2021-2025 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COLOR_COMMANDS_H
#define MUTT_COLOR_COMMANDS_H

#include "config.h"
#include "core/lib.h"
#include "mutt/lib.h"

struct AttrColor;

/**
 * @defgroup parser_callback_api Colour Parsing API
 *
 * Prototype for a function to parse color config
 *
 * @param[in]  cmd   Command being parsed
 * @param[in]  line  Buffer containing string to be parsed
 * @param[out] ac    Colour
 * @param[out] err   Buffer for error messages
 * @retval  0 Success
 * @retval -1 Error
 *
 * @pre cmd  is not NULL
 * @pre line is not NULL
 * @pre ac   is not NULL
 * @pre err  is not NULL
 */
typedef enum CommandResult (*parser_callback_t)(const struct Command *cmd, struct Buffer *line, struct AttrColor *ac, struct Buffer *err);

enum CommandResult parse_color          (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_mono           (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_uncolor        (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_uncolor_command(const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_unmono         (const struct Command *cmd, struct Buffer *line, struct Buffer *err);

int  color_get_cid (const char *name);
void color_get_name(int cid, struct Buffer *buf);

#endif /* MUTT_COLOR_COMMANDS_H */
