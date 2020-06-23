/**
 * @file
 * Handling for email address groups
 *
 * @authors
 * Copyright (C) 2006 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2009 Rocco Rutte <pdmef@gmx.net>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include "group.h"
#include "address.h"

static struct HashTable *Groups = NULL;

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
 * @retval ptr Matching Group, or new Group (if no match)
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
    g->name = mutt_str_dup(pat);
    STAILQ_INIT(&g->rs);
    TAILQ_INIT(&g->al);
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
  mutt_addrlist_clear(&g->al);
  mutt_regexlist_free(&g->rs);
  FREE(&g->name);
  FREE(&g);
}

/**
 * mutt_grouplist_clear - Clear a GroupList
 * @param gl GroupList to clear
 */
void mutt_grouplist_clear(struct GroupList *gl)
{
  if (!gl)
    return;

  struct GroupNode *np = STAILQ_FIRST(gl);
  struct GroupNode *next = NULL;
  while (np)
  {
    group_remove(np->group);
    next = STAILQ_NEXT(np, entries);
    FREE(&np);
    np = next;
  }
  STAILQ_INIT(gl);
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
 * @param gl    GroupList to add to
 * @param group Group to add
 */
void mutt_grouplist_add(struct GroupList *gl, struct Group *group)
{
  if (!gl || !group)
    return;

  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, gl, entries)
  {
    if (np->group == group)
      return;
  }
  np = mutt_mem_calloc(1, sizeof(struct GroupNode));
  np->group = group;
  STAILQ_INSERT_TAIL(gl, np, entries);
}

/**
 * mutt_grouplist_destroy - Free a GroupList
 * @param gl GroupList to free
 */
void mutt_grouplist_destroy(struct GroupList *gl)
{
  if (!gl)
    return;

  struct GroupNode *np = STAILQ_FIRST(gl);
  struct GroupNode *next = NULL;
  while (np)
  {
    next = STAILQ_NEXT(np, entries);
    FREE(&np);
    np = next;
  }
  STAILQ_INIT(gl);
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

  struct AddressList al_new = TAILQ_HEAD_INITIALIZER(al_new);
  mutt_addrlist_copy(&al_new, al, false);
  mutt_addrlist_remove_xrefs(&g->al, &al_new);
  struct Address *a = NULL, *tmp = NULL;
  TAILQ_FOREACH_SAFE(a, &al_new, entries, tmp)
  {
    TAILQ_REMOVE(&al_new, a, entries);
    mutt_addrlist_append(&g->al, a);
  }
  assert(TAILQ_EMPTY(&al_new));
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
 * mutt_grouplist_add_addrlist - Add Address list to a GroupList
 * @param gl GroupList to add to
 * @param al Address list to add
 */
void mutt_grouplist_add_addrlist(struct GroupList *gl, struct AddressList *al)
{
  if (!gl || !al)
    return;

  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, gl, entries)
  {
    group_add_addrlist(np->group, al);
  }
}

/**
 * mutt_grouplist_remove_addrlist - Remove an AddressList from a GroupList
 * @param gl GroupList to remove from
 * @param al AddressList to remove
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_grouplist_remove_addrlist(struct GroupList *gl, struct AddressList *al)
{
  if (!gl || !al)
    return -1;

  struct GroupNode *gnp = NULL;
  STAILQ_FOREACH(gnp, gl, entries)
  {
    struct Address *a = NULL;
    TAILQ_FOREACH(a, al, entries)
    {
      mutt_addrlist_remove(&gnp->group->al, a->mailbox);
    }
    if (empty_group(gnp->group))
    {
      group_remove(gnp->group);
    }
  }

  return 0;
}

/**
 * mutt_grouplist_add_regex - Add matching Addresses to a GroupList
 * @param gl    GroupList to add to
 * @param s     Address to match
 * @param flags Flags, e.g. REG_ICASE
 * @param err   Buffer for error message
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_grouplist_add_regex(struct GroupList *gl, const char *s, int flags, struct Buffer *err)
{
  if (!gl || !s)
    return -1;

  int rc = 0;

  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, gl, entries)
  {
    rc = group_add_regex(np->group, s, flags, err);
    if (rc)
      return rc;
  }
  return rc;
}

/**
 * mutt_grouplist_remove_regex - Remove matching addresses from a GroupList
 * @param gl GroupList to remove from
 * @param s  Address to match
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_grouplist_remove_regex(struct GroupList *gl, const char *s)
{
  if (!gl || !s)
    return -1;

  int rc = 0;
  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, gl, entries)
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
  struct Address *a = NULL;
  TAILQ_FOREACH(a, &g->al, entries)
  {
    if (a->mailbox && mutt_istr_equal(s, a->mailbox))
      return true;
  }

  return false;
}
