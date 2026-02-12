/**
 * @file
 * Handling for email address groups
 *
 * @authors
 * Copyright (C) 2017-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018 Bo Yu <tsu.yubo@gmail.com>
 * Copyright (C) 2018-2019 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2019 Federico Kircheis <federico.kircheis@gmail.com>
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
 * @page address_group Address groups
 *
 * Handling for email address groups
 */

#include "config.h"
#include <stdbool.h>
#include <stdlib.h>
#include "group.h"
#include "address.h"

/**
 * group_free - Free an Address Group
 * @param ptr Group to free
 */
static void group_free(struct Group **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Group *g = *ptr;

  mutt_addrlist_clear(&g->al);
  mutt_regexlist_free(&g->rs);
  FREE(&g->name);

  FREE(ptr);
}

/**
 * group_new - Create a new Address Group
 * @param  name Group name
 * @retval ptr New Address Group
 *
 * @note The pattern will be copied
 */
static struct Group *group_new(const char *name)
{
  struct Group *g = MUTT_MEM_CALLOC(1, struct Group);

  g->name = mutt_str_dup(name);
  STAILQ_INIT(&g->rs);
  TAILQ_INIT(&g->al);

  return g;
}

/**
 * group_hash_free - Free our hash table data - Implements ::hash_hdata_free_t - @ingroup hash_hdata_free_api
 */
static void group_hash_free(int type, void *obj, intptr_t data)
{
  struct Group *g = obj;
  group_free(&g);
}

/**
 * group_remove - Remove a Group from the Hash Table
 * @param groups Groups HashTable
 * @param g      Group to remove
 */
static void group_remove(struct HashTable *groups, struct Group *g)
{
  if (!groups || !g)
    return;
  mutt_hash_delete(groups, g->name, g);
}

/**
 * group_is_empty - Is a Group empty?
 * @param g Group to test
 * @retval true The Group is empty
 */
static bool group_is_empty(struct Group *g)
{
  if (!g)
    return true;
  return TAILQ_EMPTY(&g->al) && STAILQ_EMPTY(&g->rs);
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
  ASSERT(TAILQ_EMPTY(&al_new));
}

/**
 * group_add_regex - Add a Regex to a Group
 * @param g     Group to add to
 * @param str   Regex string to add
 * @param flags Flags, e.g. REG_ICASE
 * @param err   Buffer for error message
 * @retval  0 Success
 * @retval -1 Error
 */
static int group_add_regex(struct Group *g, const char *str, uint16_t flags, struct Buffer *err)
{
  return mutt_regexlist_add(&g->rs, str, flags, err);
}

/**
 * group_remove_regex - Remove a Regex from a Group
 * @param g   Group to modify
 * @param str Regex string to match
 * @retval  0 Success
 * @retval -1 Error
 */
static int group_remove_regex(struct Group *g, const char *str)
{
  return mutt_regexlist_remove(&g->rs, str);
}

/**
 * group_match - Does a string match an entry in a Group?
 * @param g   Group to match against
 * @param str String to match
 * @retval true There's a match
 */
bool group_match(struct Group *g, const char *str)
{
  if (!g || !str)
    return false;

  if (mutt_regexlist_match(&g->rs, str))
    return true;
  struct Address *a = NULL;
  TAILQ_FOREACH(a, &g->al, entries)
  {
    if (a->mailbox && mutt_istr_equal(str, buf_string(a->mailbox)))
      return true;
  }

  return false;
}

/**
 * grouplist_add_group - Add a Group to a GroupList
 * @param gl GroupList to add to
 * @param g  Group to add
 */
void grouplist_add_group(struct GroupList *gl, struct Group *g)
{
  if (!gl || !g)
    return;

  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, gl, entries)
  {
    if (np->group == g)
      return;
  }
  np = MUTT_MEM_CALLOC(1, struct GroupNode);
  np->group = g;
  STAILQ_INSERT_TAIL(gl, np, entries);
}

/**
 * grouplist_destroy - Free a GroupList
 * @param gl GroupList to free
 */
void grouplist_destroy(struct GroupList *gl)
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
 * grouplist_add_addrlist - Add Address list to a GroupList
 * @param gl GroupList to add to
 * @param al Address list to add
 */
void grouplist_add_addrlist(struct GroupList *gl, struct AddressList *al)
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
 * grouplist_add_regex - Add matching Addresses to a GroupList
 * @param gl    GroupList to add to
 * @param str   Address regex string to match
 * @param flags Flags, e.g. REG_ICASE
 * @param err   Buffer for error message
 * @retval  0 Success
 * @retval -1 Error
 */
int grouplist_add_regex(struct GroupList *gl, const char *str, uint16_t flags,
                        struct Buffer *err)
{
  if (!gl || !str)
    return -1;

  int rc = 0;

  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, gl, entries)
  {
    rc = group_add_regex(np->group, str, flags, err);
    if (rc)
      return rc;
  }
  return rc;
}

/**
 * groups_new - Create a HashTable for the Address Groups
 * @retval ptr New Groups HashTable
 */
struct HashTable *groups_new(void)
{
  struct HashTable *groups = mutt_hash_new(1031, MUTT_HASH_NO_FLAGS);

  mutt_hash_set_destructor(groups, group_hash_free, 0);

  return groups;
}

/**
 * groups_free - Free Address Groups HashTable
 * @param pptr Groups HashTable to free
 */
void groups_free(struct HashTable **pptr)
{
  mutt_hash_free(pptr);
}

/**
 * groups_get_group - Get a Group by its name
 * @param groups Groups HashTable
 * @param name   Name to find
 * @retval ptr Matching Group, or new Group (if no match)
 */
struct Group *groups_get_group(struct HashTable *groups, const char *name)
{
  if (!groups || !name)
    return NULL;

  struct Group *g = mutt_hash_find(groups, name);
  if (!g)
  {
    mutt_debug(LL_DEBUG2, "Creating group %s\n", name);
    g = group_new(name);
    mutt_hash_insert(groups, g->name, g);
  }

  return g;
}

/**
 * groups_remove_grouplist - Clear a GroupList
 * @param groups Groups HashTable
 * @param gl     GroupList to clear
 */
void groups_remove_grouplist(struct HashTable *groups, struct GroupList *gl)
{
  if (!groups || !gl)
    return;

  struct GroupNode *np = STAILQ_FIRST(gl);
  struct GroupNode *next = NULL;
  while (np)
  {
    group_remove(groups, np->group);
    next = STAILQ_NEXT(np, entries);
    FREE(&np);
    np = next;
  }
  STAILQ_INIT(gl);
}

/**
 * groups_remove_addrlist - Remove an AddressList from a GroupList
 * @param groups Groups HashTable
 * @param gl     GroupList to remove from
 * @param al     AddressList to remove
 * @retval  0 Success
 * @retval -1 Error
 */
int groups_remove_addrlist(struct HashTable *groups, struct GroupList *gl,
                           struct AddressList *al)
{
  if (!groups || !gl || !al)
    return -1;

  struct GroupNode *gnp = NULL;
  STAILQ_FOREACH(gnp, gl, entries)
  {
    struct Address *a = NULL;
    TAILQ_FOREACH(a, al, entries)
    {
      mutt_addrlist_remove(&gnp->group->al, buf_string(a->mailbox));
    }
    if (group_is_empty(gnp->group))
    {
      group_remove(groups, gnp->group);
    }
  }

  return 0;
}

/**
 * groups_remove_regex - Remove matching addresses from a GroupList
 * @param groups Groups HashTable
 * @param gl     GroupList to remove from
 * @param str    Regex string to match
 * @retval  0 Success
 * @retval -1 Error
 */
int groups_remove_regex(struct HashTable *groups, struct GroupList *gl, const char *str)
{
  if (!groups || !gl || !str)
    return -1;

  int rc = 0;
  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, gl, entries)
  {
    rc = group_remove_regex(np->group, str);
    if (group_is_empty(np->group))
      group_remove(groups, np->group);
    if (rc != 0)
      return rc;
  }
  return rc;
}
