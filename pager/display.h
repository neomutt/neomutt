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
 * struct TextSyntax - Highlighting for a line of text
 */
struct TextSyntax
{
  int color;
  int first;
  int last;
};

/**
 * struct AnsiAttr - An ANSI escape sequence
 */
struct AnsiAttr
{
  AnsiFlags attr; ///< Attributes, e.g. underline, bold, etc
  int fg;         ///< Foreground colour
  int bg;         ///< Background colour
  int pair;       ///< Curses colour pair
};

/**
 * struct QClass - Style of quoted text
 */
struct QClass
{
  size_t length;
  int index;
  int color;
  char *prefix;
  struct QClass *next, *prev;
  struct QClass *down, *up;
};

/**
 * struct Line - A line of text in the pager
 */
struct Line
{
  LOFF_T offset;
  short type;
  short continuation;
  short chunks;
  short search_cnt;
  struct TextSyntax *syntax;
  struct TextSyntax *search;
  struct QClass *quote;
  unsigned int is_cont_hdr; ///< this line is a continuation of the previous header line
};

int display_line(FILE *fp, LOFF_T *last_pos, struct Line **line_info, int n, int *last,
                 int *max, PagerFlags flags, struct QClass **quote_list, int *q_level,
                 bool *force_redraw, regex_t *search_re, struct MuttWindow *win_pager);

#endif /* MUTT_PAGER_DISPLAY_H */
