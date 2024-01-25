/**
 * @file
 * A separated list of strings
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 Yousef Akbar <yousef@yhakbar.com>
 * Copyright (C) 2023 наб <nabijaczleweli@nabijaczleweli.xyz>
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
 * @page mutt_slist A separated list of strings
 *
 * A separated list of strings
 *
 * The following unused functions were removed:
 * - slist_add_list()
 * - slist_empty()
 */

#include "config.h"
#include <stddef.h>
#include "config/types.h"
#include "slist.h"
#include "buffer.h"
#include "list.h"
#include "memory.h"
#include "queue.h"
#include "string2.h"

/**
 * slist_new - Create a new string list
 * @param flags Flag to set, e.g. #D_SLIST_SEP_COMMA
 * @retval ptr New string list
 */
struct Slist *slist_new(uint32_t flags)
{
  struct Slist *list = mutt_mem_calloc(1, sizeof(*list));
  list->flags = flags;
  STAILQ_INIT(&list->head);

  return list;
}

/**
 * slist_add_string - Add a string to a list
 * @param list List to modify
 * @param str  String to add
 * @retval ptr Modified list
 */
struct Slist *slist_add_string(struct Slist *list, const char *str)
{
  if (!list)
    return NULL;

  if (str && (str[0] == '\0'))
    str = NULL;

  if (!str && !(list->flags & D_SLIST_ALLOW_EMPTY))
    return list;

  mutt_list_insert_tail(&list->head, mutt_str_dup(str));
  list->count++;

  return list;
}

/**
 * slist_equal - Compare two string lists
 * @param a First list
 * @param b Second list
 * @retval true They are identical
 */
bool slist_equal(const struct Slist *a, const struct Slist *b)
{
  if (!a && !b) /* both empty */
    return true;
  if (!a ^ !b) /* one is empty, but not the other */
    return false;
  if (a->count != b->count)
    return false;

  return mutt_list_equal(&a->head, &b->head);
}

/**
 * slist_dup - Create a copy of an Slist object
 * @param list Slist to duplicate
 * @retval ptr New Slist object
 */
struct Slist *slist_dup(const struct Slist *list)
{
  if (!list)
    return NULL;

  struct Slist *list_new = slist_new(list->flags);

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &list->head, entries)
  {
    mutt_list_insert_tail(&list_new->head, mutt_str_dup(np->data));
  }
  list_new->count = list->count;
  return list_new;
}

/**
 * slist_free - Free an Slist object
 * @param ptr Slist to free
 */
void slist_free(struct Slist **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Slist *slist = *ptr;
  mutt_list_free(&slist->head);

  FREE(ptr);
}

/**
 * slist_is_empty - Is the slist empty?
 * @param list List to check
 * @retval true List is empty
 */
bool slist_is_empty(const struct Slist *list)
{
  if (!list)
    return true;

  return list->count == 0;
}

/**
 * slist_is_member - Is a string a member of a list?
 * @param list List to modify
 * @param str  String to find
 * @retval true String is in the list
 */
bool slist_is_member(const struct Slist *list, const char *str)
{
  if (!list)
    return false;

  if (!str && !(list->flags & D_SLIST_ALLOW_EMPTY))
    return false;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &list->head, entries)
  {
    if (mutt_str_equal(np->data, str))
      return true;
  }
  return false;
}

/**
 * slist_parse - Parse a list of strings into a list
 * @param str   String of strings
 * @param flags Flags, e.g. #D_SLIST_ALLOW_EMPTY
 * @retval ptr New Slist object
 */
struct Slist *slist_parse(const char *str, uint32_t flags)
{
  char *src = mutt_str_dup(str);
  if (!src && !(flags & D_SLIST_ALLOW_EMPTY))
    return NULL;

  char sep = ' ';
  if ((flags & D_SLIST_SEP_MASK) == D_SLIST_SEP_COMMA)
    sep = ',';
  else if ((flags & D_SLIST_SEP_MASK) == D_SLIST_SEP_COLON)
    sep = ':';

  struct Slist *list = mutt_mem_calloc(1, sizeof(struct Slist));
  list->flags = flags;
  STAILQ_INIT(&list->head);

  if (!src)
    return list;

  char *start = src;
  for (char *p = start; *p; p++)
  {
    if ((p[0] == '\\') && (p[1] != '\0'))
    {
      p++;
      continue;
    }

    if (p[0] == sep)
    {
      p[0] = '\0';
      if (slist_is_member(list, start))
      {
        start = p + 1;
        continue;
      }
      mutt_list_insert_tail(&list->head, mutt_str_dup(start));
      list->count++;
      start = p + 1;
    }
  }

  if (!slist_is_member(list, start))
  {
    mutt_list_insert_tail(&list->head, mutt_str_dup(start));
    list->count++;
  }

  FREE(&src);
  return list;
}

/**
 * slist_remove_string - Remove a string from a list
 * @param list List to modify
 * @param str  String to remove
 * @retval ptr Modified list
 */
struct Slist *slist_remove_string(struct Slist *list, const char *str)
{
  if (!list)
    return NULL;
  if (!str && !(list->flags & D_SLIST_ALLOW_EMPTY))
    return list;

  struct ListNode *prev = NULL;
  struct ListNode *np = NULL;
  struct ListNode *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, &list->head, entries, tmp)
  {
    if (mutt_str_equal(np->data, str))
    {
      if (prev)
        STAILQ_REMOVE_AFTER(&list->head, prev, entries);
      else
        STAILQ_REMOVE_HEAD(&list->head, entries);
      FREE(&np->data);
      FREE(&np);
      list->count--;
      break;
    }
    prev = np;
  }
  return list;
}

/**
 * slist_to_buffer - Export an Slist to a Buffer
 * @param list List to export
 * @param buf  Buffer for the results
 * @retval num Number of strings written to Buffer
 */
int slist_to_buffer(const struct Slist *list, struct Buffer *buf)
{
  if (!list || !buf || (list->count == 0))
    return 0;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &list->head, entries)
  {
    buf_addstr(buf, np->data);
    if (STAILQ_NEXT(np, entries))
    {
      const int sep = (list->flags & D_SLIST_SEP_MASK);
      if (sep == D_SLIST_SEP_COMMA)
        buf_addch(buf, ',');
      else if (sep == D_SLIST_SEP_COLON)
        buf_addch(buf, ':');
      else
        buf_addch(buf, ' ');
    }
  }

  return list->count;
}
