/**
 * @file
 * Flags to control mutt_expando_format()
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_FORMAT_FLAGS_H
#define MUTT_FORMAT_FLAGS_H

#include <stddef.h>

/**
 * enum FormatFlag - Control the behaviour of mutt_expando_format()
 */
enum FormatFlag
{
  MUTT_FORMAT_FORCESUBJ   = (1 << 0), ///< print the subject even if unchanged
  MUTT_FORMAT_TREE        = (1 << 1), ///< draw the thread tree
  MUTT_FORMAT_MAKEPRINT   = (1 << 2), ///< make sure that all chars are printable
  MUTT_FORMAT_OPTIONAL    = (1 << 3), ///< allow optional field processing
  MUTT_FORMAT_STAT_FILE   = (1 << 4), ///< used by attach_format_str
  MUTT_FORMAT_ARROWCURSOR = (1 << 5), ///< reserve space for arrow_cursor
  MUTT_FORMAT_INDEX       = (1 << 6), ///< this is a main index entry
  MUTT_FORMAT_NOFILTER    = (1 << 7), ///< do not allow filtering on this pass
  MUTT_FORMAT_PLAIN       = (1 << 8), ///< do not prepend DISP_TO, DISP_CC ...
};

/**
 * typedef format_t - Prototype for a mutt_expando_format() callback function
 * @param[out] buf      Buffer in which to save string
 * @param[in]  buflen   Buffer length
 * @param[in]  col      Starting column
 * @param[in]  cols     Number of screen columns
 * @param[in]  op       printf-like operator, e.g. 't'
 * @param[in]  src      printf-like format string
 * @param[in]  prec     Field precision, e.g. "-3.4"
 * @param[in]  if_str   If condition is met, display this string
 * @param[in]  else_str Otherwise, display this string
 * @param[in]  data     Pointer to the mailbox Context
 * @param[in]  flags    Format flags
 * @retval src (unchanged)
 *
 * Each callback function implements some expandos, e.g.
 *
 * | Expando | Description
 * |:--------|:-----------
 * | \%t     | Title
 */
typedef const char *format_t(char *buf, size_t buflen, size_t col, int cols,
                             char op, const char *src, const char *prec,
                             const char *if_str, const char *else_str,
                             unsigned long data, enum FormatFlag flags);

#endif /* MUTT_FORMAT_FLAGS_H */
