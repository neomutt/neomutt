/**
 * @file
 * Shared code
 *
 * @authors
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
 * Copyright (C) 2023-2024 Richard Russon <rich@flatcap.org>
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
 * @page expando_helpers Shared code
 *
 * Shared code
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "helpers.h"
#include "mutt_thread.h"
#include "render.h"

/**
 * find_get_number - Find a get_number() callback function
 * @param erc   Render callbacks to search
 * @param did   Domain ID to match
 * @param uid   Unique ID to match
 * @retval ptr Matching Render data
 */
const struct ExpandoRenderCallback *find_get_number(const struct ExpandoRenderCallback *erc,
                                                    int did, int uid)
{
  if (!erc)
    return NULL;

  for (; erc->did != -1; erc++)
  {
    if ((erc->did == did) && (erc->uid == uid) && erc->get_number)
    {
      return erc;
    }
  }

  return NULL;
}

/**
 * find_get_string - Find a get_string() callback function
 * @param erc   Render callbacks to search
 * @param did   Domain ID to match
 * @param uid   Unique ID to match
 * @retval ptr Matching Render data
 */
const struct ExpandoRenderCallback *find_get_string(const struct ExpandoRenderCallback *erc,
                                                    int did, int uid)
{
  if (!erc)
    return NULL;

  for (; erc->did != -1; erc++)
  {
    if ((erc->did == did) && (erc->uid == uid) && erc->get_string)
    {
      return erc;
    }
  }

  return NULL;
}

/**
 * buf_lower_special - Convert to lowercase, excluding special characters
 * @param buf String to lowercase
 *
 * The string is transformed in place.
 */
void buf_lower_special(struct Buffer *buf)
{
  if (!buf || !buf->data)
    return;

  char *p = buf->data;

  while (*p)
  {
    if (*p == MUTT_SPECIAL_INDEX)
    {
      p += 2;
      continue;
    }
    else if (*p < MUTT_TREE_MAX)
    {
      p++;
      continue;
    }

    *p = mutt_tolower((unsigned char) *p);
    p++;
  }
}
