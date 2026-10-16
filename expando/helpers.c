/**
 * @file
 * Shared code
 *
 * @authors
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
 * Copyright (C) 2023-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2025 Thomas Klausner <wiz@gatalith.at>
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
#include "gui/lib.h"
#include "helpers.h"
#include "expando.h"
#include "node.h"
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
      if (!*(p + 1))
        break;
      p += 2;
      continue;
    }
    else if (*p < MUTT_TREE_MAX)
    {
      p++;
      continue;
    }

    *p = mutt_tolower(*p);
    p++;
  }
}

/**
 * node_contains - Does the ExpandoNode contain a given ID?
 * @param node ExpandoNode
 * @param did  Domain ID, e.g. #ED_ALIAS
 * @param uid  UID, e.g. #ED_ALI_COMMENT
 *
 * Recursively search for an ExpandoNode matching (did,uid).
 */
static bool node_contains(const struct ExpandoNode *node, int did, int uid)
{
  if (!node)
    return false;

  if ((node->did == did) && (node->uid == uid))
    return true;

  struct ExpandoNode **enp = NULL;
  ARRAY_FOREACH(enp, &node->children)
  {
    if (node_contains(*enp, did, uid))
      return true;
  }

  return false;
}

/**
 * expando_contains - Does the Expando contain a given ID?
 * @param exp Expando
 * @param did Domain ID, e.g. #ED_ALIAS
 * @param uid UID, e.g. #ED_ALI_COMMENT
 *
 * Recursively search for an ExpandoNode matching (did,uid).
 */
bool expando_contains(const struct Expando *exp, int did, int uid)
{
  if (!exp)
    return false;

  return node_contains(exp->node, did, uid);
}
