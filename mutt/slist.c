/**
 * @file
 * A separated list of strings
 *
 * @authors
 * Copyright (C) 2018-2019 Richard Russon <rich@flatcap.org>
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
 * @page slist A separated list of strings
 *
 * A separated list of strings
 */

#include "config.h"
#include <stddef.h>
#include "slist.h"
#include "list.h"
#include "memory.h"
#include "queue.h"
#include "string2.h"

/**
 * slist_add_list - Add a list to another list
 * @param list String list to add to
 * @param add  String list to add
 * @retval ptr Modified list
 */
struct Slist *slist_add_list(struct Slist *list, const struct Slist *add)
{
  if (!add)
    return list;
  if (!list)
    return slist_dup(add);

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &add->head, entries)
  {
    mutt_list_insert_tail(&list->head, mutt_str_dup((char *) np->data));
    list->count++;
  }
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

  if (!str && !(list->flags & SLIST_ALLOW_EMPTY))
    return list;

  mutt_list_insert_tail(&list->head, mutt_str_dup(str));
  list->count++;

  return list;
}

/**
 * slist_compare - Compare two string lists
 * @param a First list
 * @param b Second list
 * @retval true If they are identical
 */
bool slist_compare(const struct Slist *a, const struct Slist *b)
{
  if (!a && !b) /* both empty */
    return true;
  if (!a ^ !b) /* one is empty, but not the other */
    return false;
  if (a->count != b->count)
    return false;

  return mutt_list_compare(&a->head, &b->head);
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

  struct Slist *l = mutt_mem_calloc(1, sizeof(*l));
  l->flags = list->flags;
  STAILQ_INIT(&l->head);

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &list->head, entries)
  {
    mutt_list_insert_tail(&l->head, mutt_str_dup(np->data));
  }
  return l;
}

/**
 * slist_empty - Empty out an Slist object
 * @param list Slist to empty
 * @retval ptr New Slist object
 */
struct Slist *slist_empty(struct Slist **list)
{
  if (!list || !*list)
    return NULL;

  mutt_list_free(&(*list)->head);

  if ((*list)->flags & SLIST_ALLOW_EMPTY)
  {
    (*list)->count = 0;
    return *list;
  }

  FREE(list);
  return NULL;
}

/**
 * slist_free - Free an Slist object
 * @param list Slist to free
 */
void slist_free(struct Slist **list)
{
  if (!list || !*list)
    return;

  mutt_list_free(&(*list)->head);
  FREE(list);
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

  if (!str && !(list->flags & SLIST_ALLOW_EMPTY))
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
 * @param flags Flags, e.g. #SLIST_ALLOW_EMPTY
 * @retval ptr New Slist object
 */
struct Slist *slist_parse(const char *str, int flags)
{
  char *src = mutt_str_dup(str);
  if (!src && !(flags & SLIST_ALLOW_EMPTY))
    return NULL;

  char sep = ' ';
  if ((flags & SLIST_SEP_MASK) == SLIST_SEP_COMMA)
    sep = ',';
  else if ((flags & SLIST_SEP_MASK) == SLIST_SEP_COLON)
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
      mutt_list_insert_tail(&list->head, mutt_str_dup(start));
      list->count++;
      start = p + 1;
    }
  }

  mutt_list_insert_tail(&list->head, mutt_str_dup(start));
  list->count++;

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
  if (!str && !(list->flags & SLIST_ALLOW_EMPTY))
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
