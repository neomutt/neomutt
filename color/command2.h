/**
 * @file
 * Parse colour commands
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COLOR_COMMAND_H
#define MUTT_COLOR_COMMAND_H

#include "config.h"
#include <stdint.h>
#include "core/lib.h"

struct Buffer;

/**
 * typedef parser_callback_t - Prototype for a function to parse color config
 * @param[in]  buf   Temporary Buffer space
 * @param[in]  s     Buffer containing string to be parsed
 * @param[out] fg    Foreground colour (set to -1)
 * @param[out] bg    Background colour (set to -1)
 * @param[out] attrs Attribute flags
 * @param[out] err   Buffer for error messages
 * @retval  0 Success
 * @retval -1 Error
 */
typedef int (*parser_callback_t)(struct Buffer *buf, struct Buffer *s, uint32_t *fg,
                                 uint32_t *bg, int *attrs, struct Buffer *err);

enum CommandResult mutt_parse_color  (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_parse_mono   (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_parse_uncolor(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_parse_unmono (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);

int fgbgattr_to_color(int fg, int bg, int attrs);

#endif /* MUTT_COLOR_COMMAND_H */
