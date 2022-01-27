/**
 * @file
 * ANSI Colours
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

#ifndef MUTT_COLOR_ANSI_H
#define MUTT_COLOR_ANSI_H

#include <stdbool.h>

struct AttrColorList;

/**
 * struct AnsiColor - An ANSI escape sequence
 */
struct AnsiColor
{
  struct AttrColor *attr_color;    ///< Curses colour of text
  int attrs;                       ///< Attributes, e.g. A_BOLD
  int fg;                          ///< Foreground colour
  int bg;                          ///< Background colour
};

int ansi_color_parse     (const char *str, struct AnsiColor *ansi, struct AttrColorList *acl, bool dry_run);
int ansi_color_seq_length(const char *str);

#endif /* MUTT_COLOR_ANSI_H */
