/**
 * @file
 * Information about the content of an attachment
 *
 * @authors
 * Copyright (C) 2017-2019 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2022 Michal Siedlaczek <michal@siedlaczek.me>
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

#ifndef MUTT_EMAIL_CONTENT_H
#define MUTT_EMAIL_CONTENT_H

#include <stdbool.h>

/**
 * struct Content - Info about an attachment
 *
 * Information that helps in determining the Content-* of an attachment
 */
struct Content
{
  long hibin;      ///< 8-bit characters
  long lobin;      ///< Unprintable 7-bit chars (eg., control chars)
  long nulbin;     ///< Null characters (0x0)
  long crlf;       ///< `\r` and `\n` characters
  long ascii;      ///< Number of ascii chars
  long linemax;    ///< Length of the longest line in the file
  bool space  : 1; ///< Whitespace at the end of lines?
  bool binary : 1; ///< Long lines, or CR not in CRLF pair
  bool from   : 1; ///< Has a line beginning with "From "?
  bool dot    : 1; ///< Has a line consisting of a single dot?
  bool cr     : 1; ///< Has CR, even when in a CRLF pair
};

/**
 * struct ContentState - Info about the body of an email
 *
 * Holds the state of content information detection after processing a certain
 * number of characters.
 */
struct ContentState
{
  bool from;      ///< Is the current line a prefix of "From "?
  int whitespace; ///< Number of trailing whitespaces
  bool dot;       ///< Was the last character a dot?
  int linelen;    ///< Length of the current line
  bool was_cr;    ///< Was the last character CR?
};

#endif /* MUTT_EMAIL_CONTENT_H */
