/**
 * @file
 * Color and attribute parsing
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COLOR_H
#define MUTT_COLOR_H

#include <stdint.h>
#include "mutt_commands.h"

struct Buffer;

void ci_start_color(void);
int  mutt_alloc_color(uint32_t fg, uint32_t bg);
int  mutt_combine_color(uint32_t fg_attr, uint32_t bg_attr);
void mutt_color_free(uint32_t fg, uint32_t bg);
void mutt_colors_free(void);
enum CommandResult mutt_parse_color(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
enum CommandResult mutt_parse_mono(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
enum CommandResult mutt_parse_uncolor(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
enum CommandResult mutt_parse_unmono(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);

#endif /* MUTT_COLOR_H */
