/**
 * @file
 * Pager Dialog
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
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "lib.h"

struct MuttWindow;

// clang-format off
typedef uint8_t AnsiFlags;      ///< Flags, e.g. #ANSI_OFF
#define ANSI_NO_FLAGS        0  ///< No flags are set
#define ANSI_OFF       (1 << 0) ///< Turn off colours and attributes
#define ANSI_BLINK     (1 << 1) ///< Blinking text
#define ANSI_BOLD      (1 << 2) ///< Bold text
#define ANSI_UNDERLINE (1 << 3) ///< Underlined text
#define ANSI_REVERSE   (1 << 4) ///< Reverse video
#define ANSI_COLOR     (1 << 5) ///< Use colours
// clang-format on

/**
 * struct TextSyntax - Highlighting for a piece of text
 */
struct TextSyntax
{
  int color; ///< Curses colour of text
  int first; ///< First character in line to be coloured
  int last;  ///< Last character in line to be coloured
};

/**
 * struct AnsiAttr - An ANSI escape sequence
 */
struct AnsiAttr
{
  AnsiFlags attr; ///< Attributes, e.g. #ANSI_BOLD
  int fg;         ///< ANSI Foreground colour, e.g. 1 red, 2 green, etc
  int bg;         ///< ANSI Background colour, e.g. 1 red, 2 green, etc
  int pair;       ///< Curses colour pair
};

/**
 * struct QClass - Style of quoted text
 *
 * NeoMutt will store a tree of all the different email quoting levels it
 * detects in an Email.  If $quote_regex matches, say both "> " and "| ",
 * and the Email has three levels of indent, then the tree will contain two
 * siblings each with a child and grandchild.
 */
struct QClass
{
  int quote_n;                ///< The quoteN colour index for this level
  int color;                  ///< Curses colour pair
  char *prefix;               ///< Prefix string, e.g. "> "
  size_t prefix_len;          ///< Length of the prefix string
  struct QClass *prev, *next; ///< Different quoting styles at the same level
  struct QClass *up, *down;   ///< Parent (less quoted) and child (more quoted) levels
};

/**
 * struct Line - A line of text in the pager
 */
struct Line
{
  LOFF_T offset;             ///< Offset into Email file (PagerPrivateData->fp)
  short color;               ///< Default line colour, e.g. #MT_COLOR_QUOTED
  bool cont_line   : 1;      ///< Continuation of a previous line (wrapped by NeoMutt)
  bool cont_header : 1;      ///< Continuation of a header line (wrapped by MTA)

  short syntax_arr_size;     ///< Number of items in syntax array
  struct TextSyntax *syntax; ///< Array of coloured text in the line

  short search_arr_size;     ///< Number of items in search array
  struct TextSyntax *search; ///< Array of search text in the line

  struct QClass *quote;      ///< Quoting style for this line (pointer into PagerPrivateData->quote_list)
};

int display_line(FILE *fp, LOFF_T *last_pos, struct Line **lines, int n, int *last,
                 int *max, PagerFlags flags, struct QClass **quote_list, int *q_level,
                 bool *force_redraw, regex_t *search_re, struct MuttWindow *win_pager);

#endif /* MUTT_PAGER_DISPLAY_H */
