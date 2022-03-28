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

/**
 * @page email_tags Email tags
 *
 * Driver based email tags
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "tags.h"

struct HashTable *TagTransforms; ///< Lookup table of alternative tag names
struct HashTable *TagFormats;    ///< Hash Table of tag-formats (tag -> format string)

/**
 * driver_tags_getter - Get transformed tags
 * @param head             List of tags
 * @param show_hidden      Show hidden tags
 * @param show_transformed Show transformed tags
 * @param filter           Match tags to this string
 * @retval ptr String list of tags
 *
 * Return a new allocated string containing tags separated by space
 */
static char *driver_tags_getter(struct TagList *head, bool show_hidden,
                                bool show_transformed, const char *filter)
{
  if (!head)
    return NULL;

  char *tags = NULL;
  struct Tag *np = NULL;
  STAILQ_FOREACH(np, head, entries)
  {
    if (filter && !mutt_str_equal(np->name, filter))
      continue;
    if (show_hidden || !np->hidden)
    {
      if (show_transformed && np->transformed)
        mutt_str_append_item(&tags, np->transformed, ' ');
      else
        mutt_str_append_item(&tags, np->name, ' ');
    }
  }
  return tags;
}

/**
 * driver_tags_add - Add a tag to header
 * @param[in] list    List of tags
 * @param[in] new_tag String representing the new tag
 *
 * Add a tag to the header tags
 *
 * @note The ownership of the string is passed to the TagList structure
 */
void driver_tags_add(struct TagList *list, char *new_tag)
{
  char *new_tag_transformed = mutt_hash_find(TagTransforms, new_tag);

  struct Tag *tn = mutt_mem_calloc(1, sizeof(struct Tag));
  tn->name = new_tag;
  tn->hidden = false;
  tn->transformed = mutt_str_dup(new_tag_transformed);

  /* filter out hidden tags */
  const struct Slist *c_hidden_tags =
      cs_subset_slist(NeoMutt->sub, "hidden_tags");
  if (c_hidden_tags)
    if (mutt_list_find(&c_hidden_tags->head, new_tag))
      tn->hidden = true;

  STAILQ_INSERT_TAIL(list, tn, entries);
}

/**
 * driver_tags_free - Free tags from a header
 * @param[in] list List of tags
 *
 * Free the whole tags structure
 */
void driver_tags_free(struct TagList *list)
{
  if (!list)
    return;

  struct Tag *np = STAILQ_FIRST(list);
  struct Tag *next = NULL;
  while (np)
  {
    next = STAILQ_NEXT(np, entries);
    FREE(&np->name);
    FREE(&np->transformed);
    FREE(&np);
    np = next;
  }
  STAILQ_INIT(list);
}

/**
 * driver_tags_get_transformed - Get transformed tags
 * @param[in] list List of tags
 * @retval ptr String list of tags
 *
 * Return a new allocated string containing all tags separated by space with
 * transformation
 */
char *driver_tags_get_transformed(struct TagList *list)
{
  return driver_tags_getter(list, false, true, NULL);
}

/**
 * driver_tags_get - Get tags
 * @param[in] list List of tags
 * @retval ptr String list of tags
 *
 * Return a new allocated string containing all tags separated by space
 */
char *driver_tags_get(struct TagList *list)
{
  return driver_tags_getter(list, false, false, NULL);
}

/**
 * driver_tags_get_with_hidden - Get tags with hiddens
 * @param[in] list List of tags
 * @retval ptr String list of tags
 *
 * Return a new allocated string containing all tags separated by space even
 * the hiddens.
 */
char *driver_tags_get_with_hidden(struct TagList *list)
{
  return driver_tags_getter(list, true, false, NULL);
}

/**
 * driver_tags_get_transformed_for - Get transformed tag for a tag name from a header
 * @param[in] head List of tags
 * @param[in] name Tag to transform
 * @retval ptr String tag
 *
 * Return a new allocated string containing all tags separated by space even
 * the hiddens.
 */
char *driver_tags_get_transformed_for(struct TagList *head, const char *name)
{
  return driver_tags_getter(head, true, true, name);
}

/**
 * driver_tags_replace - Replace all tags
 * @param[in] head List of tags
 * @param[in] tags string of all tags separated by space
 * @retval false No changes are made
 * @retval true  Tags are updated
 *
 * Free current tags structures and replace it by new tags
 */
bool driver_tags_replace(struct TagList *head, const char *tags)
{
  if (!head)
    return false;

  driver_tags_free(head);

  if (tags)
  {
    struct ListHead hsplit = STAILQ_HEAD_INITIALIZER(hsplit);
    mutt_list_str_split(&hsplit, tags, ' ');
    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &hsplit, entries)
    {
      driver_tags_add(head, np->data);
    }
    mutt_list_clear(&hsplit);
  }
  return true;
}

/**
 * tags_deleter - Delete a tag - Implements ::hash_hdata_free_t - @ingroup hash_hdata_free_api
 */
static void tags_deleter(int type, void *obj, intptr_t data)
{
  FREE(&obj);
}

/**
 * driver_tags_init - Initialize structures used for tags
 */
void driver_tags_init(void)
{
  TagTransforms = mutt_hash_new(64, MUTT_HASH_STRCASECMP | MUTT_HASH_STRDUP_KEYS);
  mutt_hash_set_destructor(TagTransforms, tags_deleter, 0);

  TagFormats = mutt_hash_new(64, MUTT_HASH_STRDUP_KEYS);
  mutt_hash_set_destructor(TagFormats, tags_deleter, 0);
}

/**
 * driver_tags_init - Deinitialize structures used for tags
 */
void driver_tags_cleanup(void)
{
  mutt_hash_free(&TagFormats);
  mutt_hash_free(&TagTransforms);
}
