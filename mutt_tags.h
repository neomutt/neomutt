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

/**
 * struct TagNode - Tag element
 *
 * Keep a linked list of header tags and their transformed values.
 * Textual tags can be transformed to symbols to save space.
 *
 * @sa TagNodes
 */
struct TagNode
{
  char *name;
  char *transformed;
  struct TagNode *next;
};

/**
 * struct TagHead - tags data attached to an email
 *
 * This stores all tags data associated with an email.
 */
struct TagHead
{
  /* Without hidden tags */
  char *tags;
  char *tags_transformed;

  /* With hidden tags */
  char *tags_with_hidden;
  struct TagNode *tag_list;
};

void driver_tags_free(struct TagHead *head);
const char *driver_tags_get(struct TagHead *head);
const char *driver_tags_get_with_hidden(struct TagHead *head);
const char *driver_tags_get_transformed(struct TagHead *head);
const char *driver_tags_get_transformed_for(char *name, struct TagHead *head);
void driver_tags_init(struct TagHead *head);
int driver_tags_replace(struct TagHead *head, char *tags);

#endif /* _MUTT_TAG_H */
