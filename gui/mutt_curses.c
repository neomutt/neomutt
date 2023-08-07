/**
 * @file
 * Define wrapper functions around Curses
 *
 * @authors
 * Copyright (C) 1996-2000,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
 * @page gui_curses Wrapper around Curses
 *
 * Wrapper functions around Curses
 */

#include "config.h"
#include <stddef.h>
#include "mutt_curses.h"
#include "color/lib.h"

/**
 * mutt_curses_set_color - Set the colour and attributes for text
 * @param ac Colour and Attributes to set
 */
void mutt_curses_set_color(const struct AttrColor *ac)
{
  if (!ac)
    return;

  int index = ac->curses_color ? ac->curses_color->index : 0;

#if defined(HAVE_SETCCHAR) && defined(HAVE_BKGRNDSET)
  cchar_t cch = { 0 };
  setcchar(&cch, L" ", ac->attrs, index, NULL);
  bkgrndset(&cch);
#elif defined(HAVE_BKGDSET)
  bkgdset(COLOR_PAIR(index) | ac->attrs | ' ');
#else
  attrset(COLOR_PAIR(index) | ac->attrs);
#endif
}

/**
 * mutt_curses_set_normal_backed_color_by_id - Set the colour and attributes by the colour id
 * @param cid Colour Id, e.g. #MT_COLOR_WARNING
 * @retval ptr Colour set
 *
 * @note Colour will be merged over #MT_COLOR_NORMAL
 */
const struct AttrColor *mutt_curses_set_normal_backed_color_by_id(enum ColorId cid)
{
  const struct AttrColor *ac_normal = simple_color_get(MT_COLOR_NORMAL);
  const struct AttrColor *ac_color = simple_color_get(cid);

  const struct AttrColor *ac_merge = merged_color_overlay(ac_normal, ac_color);

  mutt_curses_set_color(ac_merge);
  return ac_merge;
}

/**
 * mutt_curses_set_color_by_id - Set the colour and attributes by the colour id
 * @param cid Colour Id, e.g. #MT_COLOR_TREE
 * @retval ptr Colour set
 */
const struct AttrColor *mutt_curses_set_color_by_id(enum ColorId cid)
{
  struct AttrColor *ac = simple_color_get(cid);
  if (!attr_color_is_set(ac))
    ac = simple_color_get(MT_COLOR_NORMAL);

  mutt_curses_set_color(ac);
  return ac;
}

/**
 * mutt_curses_set_cursor - Set the cursor state
 * @param state State to set, e.g. #MUTT_CURSOR_INVISIBLE
 * @retval enum Old state, e.g. #MUTT_CURSOR_VISIBLE
 */
enum MuttCursorState mutt_curses_set_cursor(enum MuttCursorState state)
{
  static enum MuttCursorState SavedCursor = MUTT_CURSOR_VISIBLE;

  enum MuttCursorState OldCursor = SavedCursor;
  SavedCursor = state;

  if (curs_set(state) == ERR)
  {
    if (state == MUTT_CURSOR_VISIBLE)
      curs_set(MUTT_CURSOR_VERY_VISIBLE);
  }

  return OldCursor;
}
