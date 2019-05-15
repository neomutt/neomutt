/**
 * @file
 * Handling for email address groups
 *
 * @authors
 * Copyright (C) 2006 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2009 Rocco Rutte <pdmef@gmx.net>
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
 * @page addr_group Handling for email address groups
 *
 * Handling for email address groups
 */

#include "config.h"
#include <stdbool.h>
#include <stdlib.h>
#include "address/lib.h"
#include "group.h"

static struct Hash *Groups = NULL;

/**
 * mutt_grouplist_init - Initialize the GroupList singleton
 *
 * This is called once from init.c when initializing the global structures.
 */
void mutt_grouplist_init(void)
{
  Groups = mutt_hash_new(1031, MUTT_HASH_NO_FLAGS);
}

/**
 * mutt_grouplist_free - Free GroupList singleton resource
 *
 * This is called once from init.c when deinitializing the global resources.
 */
void mutt_grouplist_free(void)
{
  mutt_hash_free(&Groups);
}

/**
 * mutt_pattern_group - Match a pattern to a Group
 * @param pat Pattern to match
 * @retval ptr Matching Group
 * @retval ptr Newly created Group (if no match)
 */
struct Group *mutt_pattern_group(const char *pat)
{
  if (!pat)
    return NULL;

  struct Group *g = mutt_hash_find(Groups, pat);
  if (!g)
  {
    mutt_debug(LL_DEBUG2, "Creating group %s\n", pat);
    g = mutt_mem_calloc(1, sizeof(struct Group));
    g->name = mutt_str_strdup(pat);
    STAILQ_INIT(&g->rs);
    mutt_hash_insert(Groups, g->name, g);
  }

  return g;
}

/**
 * group_remove - Remove a Group from the Hash Table
 * @param g Group to remove
 */
static void group_remove(struct Group *g)
{
  if (!g)
    return;
  mutt_hash_delete(Groups, g->name, g);
  mutt_addresslist_free_all(&g->al);
  mutt_regexlist_free(&g->rs);
  FREE(&g->name);
  FREE(&g);
}

/**
 * mutt_grouplist_clear - Clear a GroupList
 * @param head GroupList to clear
 */
void mutt_grouplist_clear(struct GroupList *head)
{
  if (!head)
    return;

  struct GroupNode *np = STAILQ_FIRST(head);
  struct GroupNode *next = NULL;
  while (np)
  {
    group_remove(np->group);
    next = STAILQ_NEXT(np, entries);
    FREE(&np);
    np = next;
  }
  STAILQ_INIT(head);
}

/**
 * empty_group - Is a Group empty?
 * @param g Group to test
 * @retval true If the Group is empty
 */
static bool empty_group(struct Group *g)
{
  if (!g)
    return true;
  return TAILQ_EMPTY(&g->al) && STAILQ_EMPTY(&g->rs);
}

/**
 * mutt_grouplist_add - Add a Group to a GroupList
 * @param head  GroupList to add to
 * @param group Group to add
 */
void mutt_grouplist_add(struct GroupList *head, struct Group *group)
{
  if (!head || !group)
    return;

  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, head, entries)
  {
    if (np->group == group)
      return;
  }
  np = mutt_mem_calloc(1, sizeof(struct GroupNode));
  np->group = group;
  STAILQ_INSERT_TAIL(head, np, entries);
}

/**
 * mutt_grouplist_destroy - Free a GroupList
 * @param head GroupList to free
 */
void mutt_grouplist_destroy(struct GroupList *head)
{
  if (!head)
    return;

  struct GroupNode *np = STAILQ_FIRST(head);
  struct GroupNode *next = NULL;
  while (np)
  {
    next = STAILQ_NEXT(np, entries);
    FREE(&np);
    np = next;
  }
  STAILQ_INIT(head);
}

/**
 * group_add_addrlist - Add an Address List to a Group
 * @param g  Group to add to
 * @param al Address List
 */
static void group_add_addrlist(struct Group *g, const struct AddressList *al)
{
  if (!g || !al)
    return;

  struct AddressList new = TAILQ_HEAD_INITIALIZER(new);
  mutt_addresslist_copy(&new, al, false);
  mutt_addr_remove_xrefs(&g->al, &new);
  struct AddressNode *np, *tmp;
  TAILQ_FOREACH_SAFE(np, &new, entries, tmp)
  {
    TAILQ_INSERT_TAIL(&g->al, np, entries);
  }
}

/**
 * group_remove_addrlist - Remove an Address List from a Group
 * @param g Group to modify
 * @param a Address List to remove
 * @retval  0 Success
 * @retval -1 Error
 */
static int group_remove_addrlist(struct Group *g, struct AddressList *al)
{
  if (!g || !al)
    return -1;

  struct AddressNode *np = NULL;
  TAILQ_FOREACH(np, al, entries)
  mutt_addr_remove_from_list(&g->al, np->addr->mailbox);

  return 0;
}

/**
 * group_add_regex - Add a Regex to a Group
 * @param g     Group to add to
 * @param s     Regex string to add
 * @param flags Flags, e.g. REG_ICASE
 * @param err   Buffer for error message
 * @retval  0 Success
 * @retval -1 Error
 */
static int group_add_regex(struct Group *g, const char *s, int flags, struct Buffer *err)
{
  return mutt_regexlist_add(&g->rs, s, flags, err);
}

/**
 * group_remove_regex - Remove a Regex from a Group
 * @param g Group to modify
 * @param s Regex string to match
 * @retval  0 Success
 * @retval -1 Error
 */
static int group_remove_regex(struct Group *g, const char *s)
{
  return mutt_regexlist_remove(&g->rs, s);
}

/**
 * mutt_grouplist_add_addresslist - Add Address list to a GroupList
 * @param head GroupList to add to
 * @param a    Address to add
 */
void mutt_grouplist_add_addresslist(struct GroupList *head, struct AddressList *al)
{
  if (!head || !al)
    return;

  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, head, entries)
  {
    group_add_addrlist(np->group, al);
  }
}

/**
 * mutt_grouplist_remove_addresslist - Remove an AddressList from a GroupList
 * @param head GroupList to remove from
 * @param al   AddressList to remove
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_grouplist_remove_addresslist(struct GroupList *head, struct AddressList *al)
{
  if (!head || !al)
    return -1;

  int rc = 0;
  struct GroupNode *np = NULL;

  STAILQ_FOREACH(np, head, entries)
  {
    rc = group_remove_addrlist(np->group, al);
    if (empty_group(np->group))
      group_remove(np->group);
    if (rc)
      break;
  }

  return rc;
}

/**
 * mutt_grouplist_add_regex - Add matching Addresses to a GroupList
 * @param head  GroupList to add to
 * @param s     Address to match
 * @param flags Flags, e.g. REG_ICASE
 * @param err   Buffer for error message
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_grouplist_add_regex(struct GroupList *head, const char *s, int flags,
                             struct Buffer *err)
{
  if (!head || !s)
    return -1;

  int rc = 0;

  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, head, entries)
  {
    rc = group_add_regex(np->group, s, flags, err);
    if (rc)
      return rc;
  }
  return rc;
}

/**
 * mutt_grouplist_remove_regex - Remove matching addresses from a GroupList
 * @param head GroupList to remove from
 * @param s    Address to match
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_grouplist_remove_regex(struct GroupList *head, const char *s)
{
  if (!head || !s)
    return -1;

  int rc = 0;
  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, head, entries)
  {
    rc = group_remove_regex(np->group, s);
    if (empty_group(np->group))
      group_remove(np->group);
    if (rc)
      return rc;
  }
  return rc;
}

/**
 * mutt_group_match - Does a string match an entry in a Group?
 * @param g Group to match against
 * @param s String to match
 * @retval true If there's a match
 */
bool mutt_group_match(struct Group *g, const char *s)
{
  if (!g || !s)
    return false;

  if (mutt_regexlist_match(&g->rs, s))
    return true;
  struct AddressNode *np = NULL;
  TAILQ_FOREACH(np, &g->al, entries)
  {
    if (np->addr->mailbox && (mutt_str_strcasecmp(s, np->addr->mailbox) == 0))
      return true;
  }

  return false;
}
