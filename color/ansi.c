/**
 * @file
 * ANSI Colours
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
#include "debug.h"
#include "parse_ansi.h"
#include "simple2.h"

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

  if ((ansi->fg == COLOR_DEFAULT) && (ansi->bg == COLOR_DEFAULT))
  {
    switch (ansi->attrs)
    {
      case 0:
        return;
      case A_BOLD:
        ansi->attr_color = simple_color_get(MT_COLOR_BOLD);
        return;
      case A_ITALIC:
        ansi->attr_color = simple_color_get(MT_COLOR_ITALIC);
        return;
      case A_UNDERLINE:
        ansi->attr_color = simple_color_get(MT_COLOR_UNDERLINE);
        return;
    }
  }

  struct AttrColor *ac = attr_color_list_find(acl, ansi->fg, ansi->bg, ansi->attrs);
  if (ac)
  {
    ansi->attr_color = ac;
    return;
  }

  ac = attr_color_new();
  ac->attrs = ansi->attrs;

  struct CursesColor *cc = curses_color_new(ansi->fg, ansi->bg);
  ac->curses_color = cc;
  ansi->attr_color = ac;

  TAILQ_INSERT_TAIL(acl, ac, entries);
  attr_color_list_dump(acl, "AnsiColors");
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
