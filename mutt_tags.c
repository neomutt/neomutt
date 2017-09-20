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

#include "config.h"
#include "mutt_tags.h"
#include "context.h"
#include "globals.h"
#include "lib/hash.h"
#include "lib/string2.h"
#include "header.h"
#include "mx.h"

/**
 * driver_tags_free_tag_list - free tag
 * @param[in] h: pointer to a Header struct
 *
 * Free tag
 */
static void driver_tags_free_tag_list(struct TagList **kw_list)
{
  struct TagList *tmp = NULL;

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
 * driver_tags_free - Free tags from a header
 * @param[in] h: pointer to a header struct
 *
 * Free the whole tags structure
 */
void driver_tags_free(struct Header *h)
{
  if (!h->tags)
    return;
  FREE(&h->tags->tags);
  FREE(&h->tags->tags_transformed);
  FREE(&h->tags->tags_with_hidden);
  driver_tags_free_tag_list(&h->tags->tag_list);
  FREE(h->tags);
}

/**
 * driver_tags_get_transformed - Get transformed tags from a header
 * @param[in] h: pointer to a header struct
 *
 * @return string transformed tags
 *
 * Return a string containing all transformed tags separated by space
 * without hidden tags
 */
const char *driver_tags_get_transformed(struct Header *h)
{
  if (!h || !h->tags)
    return NULL;
  if(!h->tags->tags_transformed)
    return h->tags->tags;
  return h->tags->tags_transformed;
}

/**
 * driver_tags_get - Get tags from a header
 * @param[in] h: pointer to a header struct
 *
 * @return string tags
 *
 * Return a string containing all tags separated by space with
 * hidden tags
 */
const char *driver_tags_get(struct Header *h)
{
  if (!h || !h->tags || !h->tags->tags)
    return NULL;
  return h->tags->tags;
}

/**
 * driver_tags_get_with_hidden - Get tags with hiddens from a header
 * @param[in] h: pointer to a header struct
 *
 * @return string tags
 *
 * Return a string containing all tags separated by space
 * even the hiddens.
 */
const char *driver_tags_get_with_hidden(struct Header *h)
{
  if (!h || !h->tags || !h->tags->tags_with_hidden)
    return NULL;
  return h->tags->tags_with_hidden;
}

/**
 * driver_tags_get_transformed_for - Get tranformed tag for a tag name from a header
 * @param[in] tag: char* to the tag to get the transformed version
 * @param[in] h: pointer to a header struct
 *
 * @return string tag
 *
 * Return a string containing transformed tag that match the tag
 * even if this is a hidden tags
 */
const char *driver_tags_get_transformed_for(char *name, struct Header *h)
{
  if (!h || !h->tags || !h->tags->tag_list)
    return NULL;

  struct TagList *tag = h->tags->tag_list;
  while (tag)
  {
    if (strcmp(tag->name, name) == 0) {
      if (!tag->transformed)
        return tag->name;
      else
        return tag->transformed;
    }
    tag = tag->next;
  }
  return NULL;
}

void driver_tags_init(struct Header *h)
{
  h->tags = safe_calloc(1, sizeof(struct HeaderTags));
  h->tags->tags = NULL;
  h->tags->tags_transformed = NULL;
  h->tags->tags_with_hidden = NULL;
  h->tags->tag_list = NULL;
}

/**
 * driver_tags_add - Add a tag to header
 * @param[in] h: pointer to a header struct
 * @param[in] new_tag: string representing the new tag
 *
 * Add a tag to the header tags
 */
static void driver_tags_add(struct Header *h, char *new_tag)
{
  struct TagList *ttmp = NULL;
  char *new_tag_transformed = NULL;

  new_tag_transformed = hash_find(TagTransforms, new_tag);

  ttmp = safe_calloc(1, sizeof(*ttmp));
  ttmp->name = safe_strdup(new_tag);
  if (new_tag_transformed)
    ttmp->transformed = safe_strdup(new_tag_transformed);
  ttmp->next = h->tags->tag_list;
  h->tags->tag_list = ttmp;

  /* expand the all un-transformed tag string */
  mutt_str_append_item(&h->tags->tags_with_hidden, new_tag, ' ');

  /* filter out hidden tags */
  if (HiddenTags)
  {
    char *p = strstr(HiddenTags, new_tag);
    size_t xsz = p ? mutt_strlen(new_tag) : 0;

    if (p && ((p == HiddenTags) || (*(p - 1) == ',') || (*(p - 1) == ' ')) &&
        ((*(p + xsz) == '\0') || (*(p + xsz) == ',') || (*(p + xsz) == ' ')))
      return;
  }

  /* expand the visible un-transformed tag string */
  mutt_str_append_item(&h->tags->tags, new_tag, ' ');

  /* expand the transformed tag string */
  if (new_tag_transformed)
    mutt_str_append_item(&h->tags->tags_transformed, new_tag_transformed, ' ');
  else
    mutt_str_append_item(&h->tags->tags_transformed, new_tag, ' ');
}

/**
 * driver_tags_replace - Replace all tags
 * @param[in] h: pointer to a header struct
 * @param[in] tags: string of all tags separated by space
 *
 * @retval  0 If no change are made
 * @retval  1 If tags are updated
 *
 * Free current tags structures and replace it by
 * new tags
 */
int driver_tags_replace(struct Header *h, char *tags)
{
  if (!h)
    return 0;
  if (tags && h->tags && h->tags->tags &&
      mutt_strcmp(h->tags->tags, tags) == 0)
    return 0;

  driver_tags_free(h);
  driver_tags_init(h);

  if (tags)
  {
    char *tag;
    while ((tag = strsep(&tags, " ")))
      driver_tags_add(h, tag);
    FREE(&tags);
  }
  return 1;
}

int driver_tags_editor(struct Context *ctx, const char *tags, char *buf, size_t buflen)
{
  if (ctx->mx_ops->edit_msg_tags)
    return ctx->mx_ops->edit_msg_tags(ctx, tags, buf, buflen);

  mutt_message(_("Folder doesn't support tagging, aborting."));
  return -1;
}

int driver_tags_commit(struct Context *ctx, struct Header *h, char *tags)
{
  if (ctx->mx_ops->commit_msg_tags)
    return ctx->mx_ops->commit_msg_tags(ctx, h, tags);

  mutt_message(_("Folder doesn't support tagging, aborting."));
  return -1;
}
