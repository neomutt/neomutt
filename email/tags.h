/**
 * @file
 * Driver based email tags
 *
 * @authors
 * Copyright (C) 2017 Mehdi Abaakouk <sileht@sileht.net>
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2024 Dennis Schön <mail@dennis-schoen.de>
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

extern struct HashTable *TagTransforms;
extern struct HashTable *TagFormats;

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

void        tag_free(struct Tag **ptr);
struct Tag *tag_new (void);

void driver_tags_free               (struct TagList *tl);
void driver_tags_get                (struct TagList *tl, struct Buffer *tags);
void driver_tags_get_transformed    (struct TagList *tl, struct Buffer *tags);
void driver_tags_get_transformed_for(struct TagList *tl, const char *name, struct Buffer *tags);
void driver_tags_get_with_hidden    (struct TagList *tl, struct Buffer *tags);
bool driver_tags_replace            (struct TagList *tl, const char *tags);
void driver_tags_add                (struct TagList *tl, char *tag);

void driver_tags_init(void);
void driver_tags_cleanup(void);

#endif /* MUTT_EMAIL_TAGS_H */
