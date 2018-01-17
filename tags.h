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

#ifndef _MUTT_TAG_H
#define _MUTT_TAG_H

#include <stdbool.h>
#include "mutt/mutt.h"

extern char *HiddenTags;
extern struct Hash *TagTransforms;

/**
 * struct TagNode - LinkedList Tag Element
 *
 * Keep a linked list of header tags and their transformed values.
 * Textual tags can be transformed to symbols to save space.
 */
struct TagNode
{
    char *name;
    char *transformed;
    bool hidden;
    STAILQ_ENTRY(TagNode) entries;
};
STAILQ_HEAD(TagHead, TagNode);

void  driver_tags_free(struct TagHead *head);
char *driver_tags_get(struct TagHead *head);
char *driver_tags_get_transformed_for(char *name, struct TagHead *head);
char *driver_tags_get_transformed(struct TagHead *head);
char *driver_tags_get_with_hidden(struct TagHead *head);
bool  driver_tags_replace(struct TagHead *head, char *tags);

#endif /* _MUTT_TAG_H */
