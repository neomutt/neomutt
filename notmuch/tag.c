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

/**
 * @page nm_tag Notmuch tag functions
 *
 * Notmuch tag functions
 */

#include "config.h"
#include <stddef.h>
#include <ctype.h>
#include "mutt/lib.h"
#include "notmuch/tag.h"

/**
 * nm_tag_array_free - Free all memory of a NmTags
 * @param tags NmTags being cleaned up
 */
void nm_tag_array_free(struct NmTags *tags)
{
  ARRAY_FREE(&tags->tags);
  FREE(&tags->tag_str);
}

/**
 * nm_tag_str_to_tags - Converts a comma and/or space-delimited string of tags into an array
 * @param tag_str String containing a list of tags, comma- and/or space-delimited
 * @retval obj Array containing tags represented as strings
 */
struct NmTags nm_tag_str_to_tags(const char *tag_str)
{
  char *buf = mutt_str_dup(tag_str);

  struct NmTags tags = { ARRAY_HEAD_INITIALIZER, buf };

  char *end = NULL;
  char *tag = NULL;

  for (char *p = buf; p && (p[0] != '\0'); p++)
  {
    if (!tag && isspace(*p))
      continue;
    if (!tag)
      tag = p; /* begin of the tag */
    if ((p[0] == ',') || (p[0] == ' '))
      end = p; /* terminate the tag */
    else if (p[1] == '\0')
      end = p + 1; /* end of optstr */
    if (!tag || !end)
      continue;
    if (tag >= end)
      break;
    *end = '\0';

    ARRAY_ADD(&tags.tags, tag);

    end = NULL;
    tag = NULL;
  }

  return tags;
}
