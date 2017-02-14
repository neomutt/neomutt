/*
 * Copyright (C) 2017 Mehdi Abaakouk <sileht@sileht.net>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "config.h"
#include "mutt_tags.h"
#include "globals.h"
#include "lib/hash.h"
#include "lib/string2.h"
#include "header.h"

#define HEADER_TAGS(ph) ((struct HeaderTags *) ((ph)->tags))

/**
 * hdr_tags_free_tag_list - free tag
 * @param[in] h: pointer to a header struct
 *
 * Free tag
 */
void hdr_tags_free_tag_list(struct HeaderTag **kw_list)
{
  struct HeaderTag *tmp = NULL;

  while ((tmp = *kw_list) != NULL)
  {
    *kw_list = tmp->next;
    FREE(&tmp->name);
    FREE(&tmp->transformed);
    FREE(&tmp);
  }

  *kw_list = 0;
}

/**
 * hdr_tags_free - Free tags from a header
 * @param[in] h: pointer to a header struct
 *
 * Free the whole tags structure
 */
void hdr_tags_free(struct Header *h)
{
  if (!HEADER_TAGS(h))
    return;
  FREE(&HEADER_TAGS(h)->tags);
  FREE(&HEADER_TAGS(h)->tags_transformed);
  FREE(&HEADER_TAGS(h)->tags_with_hidden);
  hdr_tags_free_tag_list(&HEADER_TAGS(h)->tag_list);
}

/**
 * hdr_tags_get_transformed - Get transformed tags from a header
 * @param[in] h: pointer to a header struct
 *
 * @return string transformed tags
 *
 * Return a string containing all transformed tags separated by space
 * without hidden tags
 */
const char *hdr_tags_get_transformed(struct Header *h)
{
  if (!h || !HEADER_TAGS(h) || !HEADER_TAGS(h)->tags_transformed)
    return NULL;
  return HEADER_TAGS(h)->tags_transformed;
}

/*
 * hdr_tags_get - Get tags from a header
 * @param[in] h: pointer to a header struct
 *
 * @return string tags
 *
 * Return a string containing all tags separated by space with
 * hidden tags
 */
const char *hdr_tags_get(struct Header *h)
{
  if (!h || !HEADER_TAGS(h) || !HEADER_TAGS(h)->tags)
    return NULL;
  return HEADER_TAGS(h)->tags;
}

/*
 * hdr_tags_get - Get tags with hiddens from a header
 * @param[in] h: pointer to a header struct
 *
 * @return string tags
 *
 * Return a string containing all tags separated by space
 * even the hiddens.
 */
const char *hdr_tags_get_with_hidden(struct Header *h)
{
  if (!h || !HEADER_TAGS(h) || !HEADER_TAGS(h)->tags_with_hidden)
    return NULL;
  return HEADER_TAGS(h)->tags_with_hidden;
}

/**
 * hdr_tags_get_transformed_for - Get tranformed tag for a tag name from a header
 * @param[in] tag: char* to the tag to get the transformed version
 * @param[in] h: pointer to a header struct
 *
 * @return string tag
 *
 * Return a string containing transformed tag that match the tag
 * even if this is a hidden tags
 */
const char *hdr_tags_get_transformed_for(char *name, struct Header *h)
{
  if (!h || !HEADER_TAGS(h) || !HEADER_TAGS(h)->tag_list)
    return NULL;

  struct HeaderTag *tag = HEADER_TAGS(h)->tag_list;
  while (tag)
  {
    if (strcmp(tag->name, name) == 0)
      return tag->transformed;
    tag = tag->next;
  }
  return NULL;
}

void hdr_tags_init(struct Header *h)
{
  h->tags = safe_calloc(1, sizeof(struct HeaderTags));
  HEADER_TAGS(h)->tags = NULL;
  HEADER_TAGS(h)->tags_transformed = NULL;
  HEADER_TAGS(h)->tags_with_hidden = NULL;
  HEADER_TAGS(h)->tag_list = NULL;
}
/**
 * hdr_tags_replace - Add a tag to header
 * @param[in] h: pointer to a header struct
 * @param[in] new_tag: string representing the new tag
 *
 * Add a tag to the header tags
 */
void hdr_tags_add(struct Header *h, char *new_tag)
{
  struct HeaderTag *ttmp = NULL;
  char *new_tag_transformed = NULL;

  new_tag_transformed = hash_find(TagTransforms, new_tag);
  if (!new_tag_transformed)
    new_tag_transformed = new_tag;

  ttmp = safe_calloc(1, sizeof(*ttmp));
  ttmp->name = safe_strdup(new_tag);
  ttmp->transformed = safe_strdup(new_tag_transformed);
  ttmp->next = HEADER_TAGS(h)->tag_list;
  HEADER_TAGS(h)->tag_list = ttmp;

  /* expand the all un-transformed tag string */
  mutt_str_append_item(&HEADER_TAGS(h)->tags_with_hidden, new_tag, ' ');

  /* filter out hidden tags */
  if (HiddenTags)
  {
    char *p = strstr(HiddenTags, new_tag);
    size_t xsz = p ? strlen(new_tag) : 0;

    if (p && ((p == HiddenTags) || (*(p - 1) == ',') || (*(p - 1) == ' ')) &&
        ((*(p + xsz) == '\0') || (*(p + xsz) == ',') || (*(p + xsz) == ' ')))
      return;
  }

  /* expand the visible un-transformed tag string */
  mutt_str_append_item(&HEADER_TAGS(h)->tags, new_tag, ' ');

  /* expand the transformed tag string */
  mutt_str_append_item(&HEADER_TAGS(h)->tags_transformed, new_tag_transformed, ' ');
}

/**
 * hdr_tags_replace - Replace all tags
 * @param[in] h: pointer to a header struct
 * @param[in] tags: string of all tags separated by space
 *
 * @retval  0 If no change are made
 * @retval  1 If tags are updated
 *
 *
 * Free current tags structures and replace it by
 * new tags
 */
int hdr_tags_replace(struct Header *h, char *tags)
{
  if (!h)
    return 0;
  if (tags && HEADER_TAGS(h) && HEADER_TAGS(h)->tags &&
      mutt_strcmp(HEADER_TAGS(h)->tags, tags) == 0)
    return 0;

  hdr_tags_free(h);

  if (tags)
  {
    char *tag;
    while ((tag = strsep(&tags, " ")))
      hdr_tags_add(h, tag);
    FREE(&tags);
  }
  return 1;
}
