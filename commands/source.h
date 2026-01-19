/**
 * @file
 * Parse Source Commands
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

#ifndef MUTT_COMMANDS_SOURCE_H
#define MUTT_COMMANDS_SOURCE_H

#include "config.h"
#include "core/lib.h"

struct Buffer;
struct ConfigParseError;
struct ParseContext;

enum CommandResult parse_source(const struct Command *cmd, struct Buffer *line,
                                struct ParseContext *pctx, struct ConfigParseError *perr);

int source_rc(const char *rcfile_path, struct Buffer *err);
int source_rc_ctx(const char *rcfile_path, struct ParseContext *pctx, struct ConfigParseError *perr);

void source_stack_cleanup(void);
enum CommandResult parse_rc_line_cwd(const char *line, char *cwd, struct Buffer *err);
enum CommandResult parse_rc_line_cwd_ctx(const char *line, const char *cwd,
                                         struct ParseContext *pctx,
                                         struct ConfigParseError *perr);
char *mutt_get_sourced_cwd(void);

#endif /* MUTT_COMMANDS_SOURCE_H */

