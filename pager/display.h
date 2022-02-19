/**
 * @file
 * Pager Display
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

#ifndef MUTT_PAGER_DISPLAY_H
#define MUTT_PAGER_DISPLAY_H

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "lib.h"

struct AttrColorList;
struct MuttWindow;

/**
 * struct TextSyntax - Highlighting for a piece of text
 */
struct TextSyntax
{
  struct AttrColor *attr_color; ///< Curses colour of text
  int first; ///< First character in line to be coloured
  int last;  ///< Last character in line to be coloured (not included)
};
ARRAY_HEAD(TextSyntaxArray, struct TextSyntax);

/**
 * struct Line - A line of text in the pager
 */
struct Line
{
  LOFF_T offset;             ///< Offset into Email file (PagerPrivateData->fp)
  short cid;                 ///< Default line colour, e.g. #MT_COLOR_QUOTED
  bool cont_line   : 1;      ///< Continuation of a previous line (wrapped by NeoMutt)
  bool cont_header : 1;      ///< Continuation of a header line (wrapped by MTA)

  short syntax_arr_size;     ///< Number of items in syntax array
  struct TextSyntax *syntax; ///< Array of coloured text in the line

  short search_arr_size;     ///< Number of items in search array
  struct TextSyntax *search; ///< Array of search text in the line

  struct QuoteStyle *quote;  ///< Quoting style for this line (pointer into PagerPrivateData->quote_list)
};

int display_line(FILE *fp, LOFF_T *bytes_read, struct Line **lines,
                 int line_num, int *lines_used, int *lines_max, PagerFlags flags,
                 struct QuoteStyle **quote_list, int *q_level, bool *force_redraw,
                 regex_t *search_re, struct MuttWindow *win_pager, struct AttrColorList *ansi_list);

#endif /* MUTT_PAGER_DISPLAY_H */
