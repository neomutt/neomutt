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
 * @page tags Driver based email tags
 *
 * Driver based email tags
 */

#include "config.h"
#include <stddef.h>
#include <string.h>
#include "mutt/mutt.h"
#include "tags.h"

char *HiddenTags;           /**< Private tags which should not be displayed */
struct Hash *TagTransforms; /**< Lookup table of alternative tag names */

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
static char *driver_tags_getter(struct TagHead *head, bool show_hidden,
                                bool show_transformed, char *filter)
{
  if (!head)
    return NULL;

  char *tags = NULL;
  struct TagNode *np;
  STAILQ_FOREACH(np, head, entries)
  {
    if (filter && mutt_str_strcmp(np->name, filter) != 0)
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
 * @param[in] head List of tags
 * @param[in] new_tag string representing the new tag
 *
 * Add a tag to the header tags
 */
static void driver_tags_add(struct TagHead *head, char *new_tag)
{
  char *new_tag_transformed = mutt_hash_find(TagTransforms, new_tag);

  struct TagNode *np = mutt_mem_calloc(1, sizeof(struct TagNode));
  np->name = mutt_str_strdup(new_tag);
  np->hidden = false;
  if (new_tag_transformed)
    np->transformed = mutt_str_strdup(new_tag_transformed);

  /* filter out hidden tags */
  if (HiddenTags)
  {
    char *p = strstr(HiddenTags, new_tag);
    size_t xsz = p ? mutt_str_strlen(new_tag) : 0;

    if (p && ((p == HiddenTags) || (*(p - 1) == ',') || (*(p - 1) == ' ')) &&
        ((*(p + xsz) == '\0') || (*(p + xsz) == ',') || (*(p + xsz) == ' ')))
    {
      np->hidden = true;
    }
  }

  STAILQ_INSERT_TAIL(head, np, entries);
}

/**
 * driver_tags_free - Free tags from a header
 * @param[in] head List of tags
 *
 * Free the whole tags structure
 */
void driver_tags_free(struct TagHead *head)
{
  if (!head)
    return;

  struct TagNode *np = STAILQ_FIRST(head), *next = NULL;
  while (np)
  {
    next = STAILQ_NEXT(np, entries);
    FREE(&np->name);
    FREE(&np->transformed);
    FREE(&np);
    np = next;
  }
  STAILQ_INIT(head);
}

/**
 * driver_tags_get_transformed - Get transformed tags
 * @param[in] head List of tags
 *
 * Return a new allocated string containing all tags separated by space with
 * transformation
 */
char *driver_tags_get_transformed(struct TagHead *head)
{
  return driver_tags_getter(head, false, true, NULL);
}

/**
 * driver_tags_get - Get tags
 * @param[in] head List of tags
 *
 * Return a new allocated string containing all tags separated by space
 */
char *driver_tags_get(struct TagHead *head)
{
  return driver_tags_getter(head, false, false, NULL);
}

/**
 * driver_tags_get_with_hidden - Get tags with hiddens
 * @param[in] head List of tags
 *
 * Return a new allocated string containing all tags separated by space even
 * the hiddens.
 */
char *driver_tags_get_with_hidden(struct TagHead *head)
{
  return driver_tags_getter(head, true, false, NULL);
}

/**
 * driver_tags_get_transformed_for - Get transformed tag for a tag name from a header
 * @param[in] name Tag to transform
 * @param[in] head List of tags
 *
 * @return string tag
 *
 * Return a new allocated string containing all tags separated by space even
 * the hiddens.
 */
char *driver_tags_get_transformed_for(char *name, struct TagHead *head)
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
bool driver_tags_replace(struct TagHead *head, char *tags)
{
  if (!head)
    return false;

  driver_tags_free(head);

  if (tags)
  {
    char *tag;
    while ((tag = strsep(&tags, " ")))
      driver_tags_add(head, tag);
    FREE(&tags);
  }
  return true;
}
