/**
 * @file
 * ANSI Colours
 *
 * @authors
 * Copyright (C) 2022-2023 Richard Russon <rich@flatcap.org>
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
#include "attr.h"

/**
 * struct AnsiColor - An ANSI escape sequence
 *
 * @note AnsiColor doesn't own the AttrColor
 */
struct AnsiColor
{
  struct ColorElement fg;              ///< Foreground colour
  struct ColorElement bg;              ///< Background colour
  int attrs;                           ///< Text attributes, e.g. A_BOLD
  const struct AttrColor *attr_color;  ///< Curses colour of text
};

int ansi_color_parse(const char *str, struct AnsiColor *ansi, struct AttrColorList *acl, bool dry_run);

#endif /* MUTT_COLOR_ANSI_H */
