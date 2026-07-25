/**
 * @file
 * Theme loading and management
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2026 Pedro Schreiber
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

#ifndef MUTT_COLOR_THEME_H
#define MUTT_COLOR_THEME_H

struct Buffer;
struct Command;
struct ParseContext;
struct ParseError;

enum CommandResult parse_theme(const struct Command *cmd, struct Buffer *line,
                               const struct ParseContext *pc, struct ParseError *pe);

#endif /* MUTT_COLOR_THEME_H */
