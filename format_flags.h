/**
 * @file
 * Flags to control mutt_FormatString()
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

#ifndef _MUTT_FORMAT_FLAGS_H
#define _MUTT_FORMAT_FLAGS_H

#include <stddef.h>

/**
 * enum FormatFlag - Control the behaviour of mutt_expando_format()
 */
enum FormatFlag
{
  MUTT_FORMAT_FORCESUBJ   = (1 << 0), /**< print the subject even if unchanged */
  MUTT_FORMAT_TREE        = (1 << 1), /**< draw the thread tree */
  MUTT_FORMAT_MAKEPRINT   = (1 << 2), /**< make sure that all chars are printable */
  MUTT_FORMAT_OPTIONAL    = (1 << 3), /**< allow optional field processing */
  MUTT_FORMAT_STAT_FILE   = (1 << 4), /**< used by mutt_attach_fmt */
  MUTT_FORMAT_ARROWCURSOR = (1 << 5), /**< reserve space for arrow_cursor */
  MUTT_FORMAT_INDEX       = (1 << 6), /**< this is a main index entry */
  MUTT_FORMAT_NOFILTER    = (1 << 7)  /**< do not allow filtering on this pass */
};

typedef const char *format_t(char *dest, size_t destlen, size_t col, int cols,
                             char op, const char *src, const char *prefix,
                             const char *ifstring, const char *elsestring,
                             unsigned long data, enum FormatFlag flags);

void mutt_expando_format(char *dest,         /* output buffer */
                       size_t destlen,     /* output buffer len */
                       size_t col,         /* starting column (nonzero when called recursively) */
                       int cols,           /* maximum columns */
                       const char *src,    /* template string */
                       format_t *callback, /* callback for processing */
                       unsigned long data, /* callback data */
                       enum FormatFlag flags); /* callback flags */

#endif /* _MUTT_FORMAT_FLAGS_H */
