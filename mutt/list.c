/**
 * @file
 * Singly-linked list type
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2017 Pietro Cerutti <gahr@gahr.ch>
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
 * @page list Singly-linked list type
 *
 * Singly-linked list of strings.
 */

#include "config.h"
#include <stdbool.h>
#include "list.h"
#include "memory.h"
#include "queue.h"
#include "string2.h"

/**
 * mutt_list_insert_head - Insert a string at the beginning of a List
 * @param h Head of the List
 * @param s String to insert
 * @retval ptr Newly inserted ListNode containing the string
 *
 * @note The inserted string isn't strdup()d
 */
struct ListNode *mutt_list_insert_head(struct ListHead *h, char *s)
{
  if (!h)
    return NULL;

  struct ListNode *np = mutt_mem_calloc(1, sizeof(struct ListNode));
  np->data = s;
  STAILQ_INSERT_HEAD(h, np, entries);
  return np;
}

/**
 * mutt_list_insert_tail - Append a string to the end of a List
 * @param h Head of the List
 * @param s String to insert
 * @retval ptr Newly appended ListNode containing the string
 *
 * @note The inserted string isn't strdup()d
 */
struct ListNode *mutt_list_insert_tail(struct ListHead *h, char *s)
{
  if (!h)
    return NULL;

  struct ListNode *np = mutt_mem_calloc(1, sizeof(struct ListNode));
  np->data = s;
  STAILQ_INSERT_TAIL(h, np, entries);
  return np;
}

/**
 * mutt_list_insert_after - Insert a string after a given ListNode
 * @param h Head of the List
 * @param n ListNode after which the string will be inserted
 * @param s String to insert
 * @retval ptr Newly created ListNode containing the string
 *
 * @note The inserted string isn't strdup()d
 */
struct ListNode *mutt_list_insert_after(struct ListHead *h, struct ListNode *n, char *s)
{
  if (!h || !n)
    return NULL;

  struct ListNode *np = mutt_mem_calloc(1, sizeof(struct ListNode));
  np->data = s;
  STAILQ_INSERT_AFTER(h, n, np, entries);
  return np;
}

/**
 * mutt_list_find - Find a string in a List
 * @param h    Head of the List
 * @param data String to find
 * @retval ptr ListNode containing the string
 * @retval NULL if the string isn't found
 */
struct ListNode *mutt_list_find(const struct ListHead *h, const char *data)
{
  if (!h)
    return NULL;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, h, entries)
  {
    if (mutt_str_equal(np->data, data))
    {
      return np;
    }
  }
  return NULL;
}

/**
 * mutt_list_free - Free a List AND its strings
 * @param h Head of the List
 */
void mutt_list_free(struct ListHead *h)
{
  if (!h)
    return;

  struct ListNode *np = STAILQ_FIRST(h);
  struct ListNode *next = NULL;
  while (np)
  {
    next = STAILQ_NEXT(np, entries);
    FREE(&np->data);
    FREE(&np);
    np = next;
  }
  STAILQ_INIT(h);
}

/**
 * mutt_list_free_type - Free a List of type
 * @param h Head of the List
 * @param fn Function to free contents of ListNode
 */
void mutt_list_free_type(struct ListHead *h, list_free_t fn)
{
  if (!h || !fn)
    return;

  struct ListNode *np = STAILQ_FIRST(h);
  struct ListNode *next = NULL;
  while (np)
  {
    next = STAILQ_NEXT(np, entries);
    fn((void **) &np->data);
    FREE(&np);
    np = next;
  }
  STAILQ_INIT(h);
}

/**
 * mutt_list_clear - Free a list, but NOT its strings
 * @param h Head of the List
 *
 * This can be used when the strings have a different lifetime to the List.
 */
void mutt_list_clear(struct ListHead *h)
{
  if (!h)
    return;

  struct ListNode *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, h, entries, tmp)
  {
    STAILQ_REMOVE(h, np, ListNode, entries);
    FREE(&np);
  }

  STAILQ_INIT(h);
}

/**
 * mutt_list_match - Is the string in the list (see notes)
 * @param s String to match
 * @param h Head of the List
 * @retval true String matches a List item (or List contains "*")
 *
 * This is a very specific function.  It searches a List of strings looking for
 * a match.  If the list contains a string "*", then it match any input string.
 *
 * @note The strings are compared to the length of the List item, e.g.
 *       List: "Red" matches Param: "Redwood", but not the other way around.
 * @note The case of the strings is ignored.
 */
bool mutt_list_match(const char *s, struct ListHead *h)
{
  if (!h)
    return false;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, h, entries)
  {
    if ((*np->data == '*') || mutt_istr_startswith(s, np->data))
      return true;
  }
  return false;
}

/**
 * mutt_list_compare - Compare two string lists
 * @param ah First string list
 * @param bh Second string list
 * @retval true Lists are identical
 *
 * To be identical, the lists must both be the same length and contain the same
 * strings.  Two empty lists are identical.
 */
bool mutt_list_compare(const struct ListHead *ah, const struct ListHead *bh)
{
  if (!ah || !bh)
    return false;

  struct ListNode *a = STAILQ_FIRST(ah);
  struct ListNode *b = STAILQ_FIRST(bh);

  while (a && b)
  {
    if (!mutt_str_equal(a->data, b->data))
      return false;

    a = STAILQ_NEXT(a, entries);
    b = STAILQ_NEXT(b, entries);
  }
  if (a || b)
    return false;

  return true;
}

/**
 * mutt_list_str_split - Split a string into a list using a separator char
 * @param head List to add to
 * @param src  String to split
 * @param sep  Word separator
 * @retval num Number of items in list
 */
size_t mutt_list_str_split(struct ListHead *head, const char *src, char sep)
{
  if (!src || (*src == '\0'))
    return 0;

  size_t count = 0;
  while (true)
  {
    const char *start = src;
    while ((*src != '\0') && (*src != sep))
      src++;

    mutt_list_insert_tail(head, mutt_strn_dup(start, src - start));
    count++;

    if ((*src == '\0'))
      break;

    src++;
  }

  return count;
}
