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

/**
 * @page color_ansi ANSI Colours
 *
 * Handle ANSI Colours used in the Pager
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "ansi.h"
#include "attr.h"
#include "color.h"
#include "curses2.h"
#include "parse_ansi.h"
#include "simple2.h"

color_t color_xterm256_to_24bit(const color_t color);

/**
 * ansi_color_list_add - Add an Ansi colour to the list
 * @param acl  List of unique colours
 * @param ansi Colour to add
 *
 * Keep track of all the unique ANSI colours in a list.
 */
static void ansi_color_list_add(struct AttrColorList *acl, struct AnsiColor *ansi)
{
  if (!acl || !ansi)
    return;

  if ((ansi->fg.color == COLOR_DEFAULT) && (ansi->bg.color == COLOR_DEFAULT))
  {
    switch (ansi->attrs)
    {
      case A_NORMAL:
        return;
      case A_BOLD:
        ansi->attr_color = simple_color_get(MT_COLOR_BOLD);
        return;
#ifdef A_ITALIC_DEFINED
      case A_ITALIC:
        ansi->attr_color = simple_color_get(MT_COLOR_ITALIC);
        return;
#endif
      case A_UNDERLINE:
        ansi->attr_color = simple_color_get(MT_COLOR_UNDERLINE);
        return;
    }
  }

  color_t fg = ansi->fg.color;
  color_t bg = ansi->bg.color;

#ifdef NEOMUTT_DIRECT_COLORS
  if ((ansi->fg.type == CT_SIMPLE) || (ansi->fg.type == CT_PALETTE))
    fg = color_xterm256_to_24bit(fg);
  else if (fg < 8)
    fg = 8;
  if ((ansi->bg.type == CT_SIMPLE) || (ansi->bg.type == CT_PALETTE))
    bg = color_xterm256_to_24bit(bg);
  else if (bg < 8)
    bg = 8;
#endif

  struct AttrColor *ac = attr_color_list_find(acl, fg, bg, ansi->attrs);
  if (ac)
  {
    ansi->attr_color = ac;
    return;
  }

  ac = attr_color_new();
  ac->attrs = ansi->attrs;
  ac->fg = ansi->fg;
  ac->bg = ansi->bg;

  struct CursesColor *cc = curses_color_new(fg, bg);
  ac->curses_color = cc;
  ansi->attr_color = ac;

  TAILQ_INSERT_TAIL(acl, ac, entries);
}

/**
 * ansi_color_parse - Parse a string of ANSI escape sequence
 * @param str     String to parse
 * @param ansi    AnsiColor for the result
 * @param acl     List to store the unique colours
 * @param dry_run If true, parse but don't save the sequence
 * @retval num Total length of the escape sequences
 *
 * Parse (multiple) ANSI sequence(s) into @a ansi.
 * If the colour hasn't been seen before, store the it in @a acl.
 */
int ansi_color_parse(const char *str, struct AnsiColor *ansi,
                     struct AttrColorList *acl, bool dry_run)
{
  int seq_len = 0;
  int total_len = 0;

  while ((seq_len = ansi_color_parse_single(str + total_len, ansi, dry_run)) != 0)
  {
    total_len += seq_len;
  }

  ansi_color_list_add(acl, ansi);

  return total_len;
}
