/**
 * @file
 * Email tags/keywords/labels
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

#include "header.h"
#include "context.h"

/**
 * struct HeaderTag - Mail Header Tags
 *
 * Keep a linked list of header tags and their transformed values.
 * Textual tags can be transformed to symbols to save space.
 *
 * @sa HeaderTags#tag_list
 */
struct HeaderTag
{
  char *name;
  char *transformed;
  struct HeaderTag *next;
};

/**
 * struct HeaderTags - tags data attached to an email
 *
 * This stores all tags data associated with an email.
 */
struct HeaderTags
{
  /* Without hidden tags */
  char *tags;
  char *tags_transformed;

  /* With hidden tags */
  char *tags_with_hidden;
  struct HeaderTag *tag_list;
};

void hdr_tags_free_tag_list(struct HeaderTag **kw_list);
void hdr_tags_free(struct Header *h);
const char *hdr_tags_get(struct Header *h);
const char *hdr_tags_get_with_hidden(struct Header *h);
const char *hdr_tags_get_transformed(struct Header *h);
const char *hdr_tags_get_transformed_for(char *name, struct Header *h);
void hdr_tags_init(struct Header *h);
void hdr_tags_add(struct Header *h, char *new_tag);
int hdr_tags_replace(struct Header *h, char *tags);
int hdr_tags_editor(struct Context *ctx, const char *tags, char *buf);
int hdr_tags_commit(struct Context *ctx, struct Header *h, char *tags);

#endif /* _MUTT_TAG_H */
