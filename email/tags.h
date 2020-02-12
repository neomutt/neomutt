/**
 * @file
 * Driver based email tags
 *
 * @authors
 * Copyright (C) 2017 Mehdi Abaakouk <sileht@sileht.net>
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

#ifndef MUTT_EMAIL_TAGS_H
#define MUTT_EMAIL_TAGS_H

#include <stdbool.h>
#include "mutt/lib.h"

/* These Config Variables are only used in email/tags.c */
extern struct Slist *C_HiddenTags;

extern struct Hash *TagTransforms;

/**
 * struct Tag - LinkedList Tag Element
 *
 * Keep a linked list of header tags and their transformed values.
 * Textual tags can be transformed to symbols to save space.
 */
struct Tag
{
  char *name;                ///< Tag name
  char *transformed;         ///< Transformed name
  bool hidden;               ///< Tag should be hidden
  STAILQ_ENTRY(Tag) entries; ///< Linked list
};
STAILQ_HEAD(TagList, Tag);

void  driver_tags_free               (struct TagList *list);
char *driver_tags_get                (struct TagList *list);
char *driver_tags_get_transformed    (struct TagList *list);
char *driver_tags_get_transformed_for(struct TagList *list, const char *name);
char *driver_tags_get_with_hidden    (struct TagList *list);
bool  driver_tags_replace            (struct TagList *list, char *tags);

#endif /* MUTT_EMAIL_TAGS_H */
