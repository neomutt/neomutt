/**
 * @file
 * Colour and attributes
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
 * @page color_attr Colour and attributes
 *
 * The colour and attributes of a graphical object are represented by an
 * AttrColor.
 */

#include "config.h"
#include <stddef.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "attr.h"
#include "curses2.h"

/**
 * attr_color_clear - Free the contents of an AttrColor
 * @param ac AttrColor to empty
 *
 * @note The AttrColor object isn't freed
 */
void attr_color_clear(struct AttrColor *ac)
{
  if (!ac)
    return;

  curses_color_free(&ac->curses_color);
  ac->attrs = 0;
}

/**
 * attr_color_free - Free an AttrColor
 * @param ptr AttrColor to free
 */
void attr_color_free(struct AttrColor **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct AttrColor *ac = *ptr;
  if (ac->ref_count > 1)
  {
    ac->ref_count--;
    *ptr = NULL;
    return;
  }

  attr_color_clear(ac);
  FREE(ptr);
}

/**
 * attr_color_new - Create a new AttrColor
 * @retval ptr New AttrColor
 */
struct AttrColor *attr_color_new(void)
{
  struct AttrColor *ac = mutt_mem_calloc(1, sizeof(*ac));

  ac->ref_count = 1;

  return ac;
}

/**
 * attr_color_list_clear - Free the contents of an AttrColorList
 * @param acl List to clear
 *
 * Free each of the AttrColors in a list.
 *
 * @note The list object isn't freed, only emptied
 */
void attr_color_list_clear(struct AttrColorList *acl)
{
  if (!acl)
    return;

  struct AttrColor *ac = NULL;
  struct AttrColor *tmp = NULL;
  TAILQ_FOREACH_SAFE(ac, acl, entries, tmp)
  {
    TAILQ_REMOVE(acl, ac, entries);
    attr_color_free(&ac);
  }
}

/**
 * attr_color_list_find - Find an AttrColor in a list
 * @param acl   List to search
 * @param fg    Foreground colour
 * @param bg    Background colour
 * @param attrs Attributes, e.g. A_UNDERLINE
 * @retval ptr Matching AttrColor
 */
struct AttrColor *attr_color_list_find(struct AttrColorList *acl, uint32_t fg,
                                       uint32_t bg, int attrs)
{
  if (!acl)
    return NULL;

  struct AttrColor *ac = NULL;
  TAILQ_FOREACH(ac, acl, entries)
  {
    if (ac->attrs != attrs)
      continue;

    struct CursesColor *cc = ac->curses_color;
    if (!cc)
      continue;

    if ((cc->fg == fg) && (cc->bg == bg))
      return ac;
  }
  return NULL;
}

/**
 * attr_color_copy - Copy a colour
 * @param ac Colour to copy
 * @retval obj Copy of the colour
 */
struct AttrColor attr_color_copy(struct AttrColor *ac)
{
  struct AttrColor copy = { 0 };
  if (ac)
    copy = *ac;

  return copy;
}

/**
 * attr_color_is_set - Is the object coloured?
 * @param ac Colour to check
 * @retval true Yes, a 'color' command has been used on this object
 */
bool attr_color_is_set(struct AttrColor *ac)
{
  if (!ac)
    return false;

  return ((ac->attrs != 0) || ac->curses_color);
}

/**
 * attr_color_match - Do the colours match?
 * @param ac1 First colour
 * @param ac2 Second colour
 * @retval true The colours and attributes match
 */
bool attr_color_match(struct AttrColor *ac1, struct AttrColor *ac2)
{
  if (!ac1 ^ !ac2) // One is set, but not the other
    return false;

  if (!ac1) // Two empty colours match
    return true;

  return ((ac1->curses_color == ac2->curses_color) && (ac1->attrs == ac2->attrs));
}
