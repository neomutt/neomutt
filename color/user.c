/**
 * @file
 * User colours
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
 * @page color_user User colours
 *
 * User-configured colours
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "user.h"
#include "color.h"

/// User-configured colours, hashed by name
struct HashTable *UserColorsName = NULL; ///< "name" -> Mapping
/// User-configured colours, hashed by id
struct HashTable *UserColorsId = NULL; ///< ColorId -> Mapping

/**
 * user_colors_cleanup - Cleanup the storage of user-configured colours
 */
void user_colors_cleanup(void)
{
  mutt_hash_free(&UserColorsName);
  mutt_hash_free(&UserColorsId);
}

/**
 * user_colors_init - Initialise the storage of user-configured colours
 */
void user_colors_init(void)
{
  UserColorsName = mutt_hash_new(80, MUTT_HASH_NONE);
  UserColorsId = mutt_hash_int_new(80, MUTT_HASH_NONE);

  struct HashElem *he = NULL;
  for (const struct ColorDefinition *def = ColorDefs; def->name; def++)
  {
    void *data = (void *) def; // drop the 'const'

    he = mutt_hash_insert(UserColorsName, def->name, data);
    he->type = 1;

    he = mutt_hash_int_insert(UserColorsId, def->cid, data);
    if (he)
      he->type = 2;
  }
}

/**
 * color_get_name - Get the name from a Colour ID
 * @param cid Colour, e.g. #MT_COLOR_HEADER
 * @param buf Buffer for result
 */
void color_get_name(int cid, struct Buffer *buf)
{
  for (const struct ColorDefinition *def = ColorDefs; def->name; def++)
  {
    if (def->cid == cid)
    {
      buf_addstr(buf, def->name);
      return;
    }
  }

  buf_add_printf(buf, "UNKNOWN %d", cid);
}

/**
 * color_get_cid - Get the Colour ID from a name
 * @param name Colour name, e.g. "warning"
 * @retval num Colour ID, e.g. #MT_COLOR_WARNING
 * @retval  -1 Name not found
 */
int color_get_cid(const char *name)
{
  for (const struct ColorDefinition *def = ColorDefs; def->name; def++)
  {
    if (mutt_str_equal(def->name, name))
      return def->cid;
  }

  return -1;
}
