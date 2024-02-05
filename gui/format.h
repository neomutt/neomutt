/**
 * @file
 * Simple string formatting
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_GUI_FORMAT_H
#define MUTT_GUI_FORMAT_H

#include <stddef.h>
#include <stdbool.h>

/**
 * enum FormatJustify - Alignment for mutt_simple_format()
 */
enum FormatJustify
{
  JUSTIFY_LEFT = -1,  ///< Left justify the text
  JUSTIFY_CENTER = 0, ///< Centre the text
  JUSTIFY_RIGHT = 1,  ///< Right justify the text
};

void mutt_format       (char *buf, size_t buflen, const char *prec, const char *s, bool arboreal);
void mutt_simple_format(char *buf, size_t buflen, int min_width, int max_width, enum FormatJustify justify, char pad_char, const char *s, size_t n, bool arboreal);

#endif /* MUTT_GUI_FORMAT_H */
