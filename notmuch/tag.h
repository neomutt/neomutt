/**
 * @file
 * Notmuch tag functions
 *
 * @authors
 * Copyright (C) 2021 Austin Ray <austin@austinray.io>
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

#ifndef MUTT_NOTMUCH_TAG_H
#define MUTT_NOTMUCH_TAG_H

#include "mutt/lib.h"

/**
 * struct TagArray - Array of Notmuch tags
 *
 * The tag pointers, in the array, point into the source string.
 * They must not be freed.
 */
struct TagArray
{
  ARRAY_HEAD(, char *) tags; ///< Tags
  char *tag_str;             ///< Source string
};

void nm_tag_array_free(struct TagArray *tags);
struct TagArray nm_tag_str_to_tags(const char *tag_string);

#endif /* MUTT_NOTMUCH_TAG_H */
