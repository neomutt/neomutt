/**
 * @file
 * Attachment helper functions
 *
 * @authors
 * Copyright (C) 2022 David Purton <dcpurton@marshwiggle.net>
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
 * @page attach_lib Attachment library
 *
 * Attachment helper functions
 */

#include <stdbool.h>
#include "email/lib.h"

/**
 * attach_body_count - Count bodies
 * @param  body    Body to start counting from
 * @param  recurse Whether to recurse into groups or not
 * @retval num     Number of bodies
 * @retval -1      Failure
 * */
int attach_body_count(struct Body *body, bool recurse)
{
  if (!body)
    return -1;

  int bodies = 0;

  for (struct Body *b = body; b; b = b->next)
  {
    bodies++;
    if (recurse)
    {
      if (b->parts)
        bodies += attach_body_count(b->parts, true);
    }
  }

  return bodies;
}

/**
 * attach_body_parent - Find the parent of a body
 * @param[in]  start        Body to start search from
 * @param[in]  start_parent Parent of start Body pointer (or NULL if none)
 * @param[in]  body         Body to find parent of
 * @param[out] body_parent  Body Parent if found
 * @retval     true         Parent body found
 * @retval     false        Parent body not found
 */
bool attach_body_parent(struct Body *start, struct Body *start_parent,
                        struct Body *body, struct Body **body_parent)
{
  if (!start || !body)
    return false;

  struct Body *b = start;

  if (b->parts)
  {
    if (b->parts == body)
    {
      *body_parent = b;
      return true;
    }
  }
  while (b)
  {
    if (b == body)
    {
      *body_parent = start_parent;
      if (start_parent)
        return true;
      else
        return false;
    }
    if (b->parts)
    {
      if (attach_body_parent(b->parts, b, body, body_parent))
        return true;
    }
    b = b->next;
  }

  return false;
}

/**
 * attach_body_previous - Find the previous body of a body
 * @param[in]  start    Body to start search from
 * @param[in]  body     Body to find previous body of
 * @param[out] previous Previous Body if found
 * @retval     true     Previous body found
 * @retval     false    Previous body not found
 */
bool attach_body_previous(struct Body *start, struct Body *body, struct Body **previous)
{
  if (!start || !body)
    return false;

  struct Body *b = start;

  while (b)
  {
    if (b->next == body)
    {
      *previous = b;
      return true;
    }
    if (b->parts)
    {
      if (attach_body_previous(b->parts, body, previous))
        return true;
    }
    b = b->next;
  }

  return false;
}
