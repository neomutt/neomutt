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

#ifndef MUTT_EXPANDO_FORMAT_H
#define MUTT_EXPANDO_FORMAT_H

#include <stdbool.h>
#include <stddef.h>

/**
 * enum FormatJustify - Alignment for format_string()
 */
enum FormatJustify
{
  JUSTIFY_LEFT   = -1,       ///< Left justify the text
  JUSTIFY_CENTER =  0,       ///< Centre the text
  JUSTIFY_RIGHT  =  1,       ///< Right justify the text
};

struct Buffer;

int format_string(struct Buffer *buf, int min_cols, int max_cols, enum FormatJustify justify, char pad_char, const char *str, size_t n, bool arboreal);

#endif /* MUTT_EXPANDO_FORMAT_H */
