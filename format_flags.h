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
#include <stdint.h>

typedef uint8_t MuttFormatFlags;         ///< Flags for mutt_expando_format(), e.g. #MUTT_FORMAT_FORCESUBJ
#define MUTT_FORMAT_NO_FLAGS          0  ///< No flags are set
#define MUTT_FORMAT_FORCESUBJ   (1 << 0) ///< Print the subject even if unchanged
#define MUTT_FORMAT_TREE        (1 << 1) ///< Draw the thread tree
#define MUTT_FORMAT_OPTIONAL    (1 << 2) ///< Allow optional field processing
#define MUTT_FORMAT_STAT_FILE   (1 << 3) ///< Used by attach_format_str
#define MUTT_FORMAT_ARROWCURSOR (1 << 4) ///< Reserve space for arrow_cursor
#define MUTT_FORMAT_INDEX       (1 << 5) ///< This is a main index entry
#define MUTT_FORMAT_NOFILTER    (1 << 6) ///< Do not allow filtering on this pass
#define MUTT_FORMAT_PLAIN       (1 << 7) ///< Do not prepend DISP_TO, DISP_CC ...

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
 * @param[in]  flags    Flags, see #MuttFormatFlags
 * @retval ptr src (unchanged)
 *
 * Each callback function implements some expandos, e.g.
 *
 * | Expando | Description
 * |:--------|:-----------
 * | \%t     | Title
 */
typedef const char *(format_t)(char *buf, size_t buflen, size_t col, int cols,
                               char op, const char *src, const char *prec,
                               const char *if_str, const char *else_str,
                               unsigned long data, MuttFormatFlags flags);

#endif /* MUTT_FORMAT_FLAGS_H */
