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
 * @page group Handling for email address groups
 *
 * Handling for email address groups
 */

#include "config.h"
#include <stdbool.h>
#include <stdlib.h>
#include "mutt/mutt.h"
#include "email/lib.h"
#include "group.h"
#include "hash.h"
#include "logging.h"
#include "memory.h"
#include "regex3.h"
#include "string2.h"

static struct Hash *Groups = NULL;

/**
 * mutt_grouplist_init - Initialize the GroupList singleton.
 *
 * This is called once from init.c when initializing the global structures.
 */
void mutt_grouplist_init(void)
{
  Groups = mutt_hash_new(1031, 0);
}

/**
 * mutt_grouplist_free - Free GroupList singleton resource.
 *
 * This is called once from init.c when deinitializing the global resources.
 */
void mutt_grouplist_free(void)
{
  mutt_hash_free(&Groups);
}

/**
 * mutt_pattern_group - Match a pattern to a Group
 * @param k Pattern to match
 * @retval ptr Matching Group
 * @retval ptr Newly created Group (if no match)
 */
struct Group *mutt_pattern_group(const char *k)
{
  struct Group *p = NULL;

  if (!k)
    return 0;

  p = mutt_hash_find(Groups, k);
  if (!p)
  {
    mutt_debug(2, "Creating group %s.\n", k);
    p = mutt_mem_calloc(1, sizeof(struct Group));
    p->name = mutt_str_strdup(k);
    STAILQ_INIT(&p->rs);
    mutt_hash_insert(Groups, p->name, p);
  }

  return p;
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
  mutt_addr_free(&g->as);
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
  struct GroupNode *np = STAILQ_FIRST(head), *next = NULL;
  while (np)
  {
    group_remove(np->g);
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
  return !g->as && STAILQ_EMPTY(&g->rs);
}

/**
 * mutt_grouplist_add - Add a Group to a GroupList
 * @param head  GroupList to add to
 * @param group Group to add
 */
void mutt_grouplist_add(struct GroupList *head, struct Group *group)
{
  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, head, entries)
  {
    if (np->g == group)
      return;
  }
  np = mutt_mem_calloc(1, sizeof(struct GroupNode));
  np->g = group;
  STAILQ_INSERT_TAIL(head, np, entries);
}

/**
 * mutt_grouplist_destroy - Free a GroupList
 * @param head GroupList to free
 */
void mutt_grouplist_destroy(struct GroupList *head)
{
  struct GroupNode *np = STAILQ_FIRST(head), *next = NULL;
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
 * @param g Group to add to
 * @param a Address List
 */
static void group_add_addrlist(struct Group *g, struct Address *a)
{
  struct Address **p = NULL, *q = NULL;

  if (!g)
    return;
  if (!a)
    return;

  for (p = &g->as; *p; p = &((*p)->next))
    ;

  q = mutt_addr_copy_list(a, false);
  q = mutt_addr_remove_xrefs(g->as, q);
  *p = q;
}

/**
 * group_remove_addrlist - Remove an Address List from a Group
 * @param g Group to modify
 * @param a Address List to remove
 * @retval  0 Success
 * @retval -1 Error
 */
static int group_remove_addrlist(struct Group *g, struct Address *a)
{
  struct Address *p = NULL;

  if (!g)
    return -1;
  if (!a)
    return -1;

  for (p = a; p; p = p->next)
    mutt_addr_remove_from_list(&g->as, p->mailbox);

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
 * mutt_grouplist_add_addrlist - Add Address list to a GroupList
 * @param head GroupList to add to
 * @param a    Address to add
 */
void mutt_grouplist_add_addrlist(struct GroupList *head, struct Address *a)
{
  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, head, entries)
  {
    group_add_addrlist(np->g, a);
  }
}

/**
 * mutt_grouplist_remove_addrlist - Remove Address from a GroupList
 * @param head GroupList to remove from
 * @param a    Address to remove
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_grouplist_remove_addrlist(struct GroupList *head, struct Address *a)
{
  int rc = 0;
  struct GroupNode *np = NULL;

  STAILQ_FOREACH(np, head, entries)
  {
    rc = group_remove_addrlist(np->g, a);
    if (empty_group(np->g))
      group_remove(np->g);
    if (rc)
      return rc;
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
  int rc = 0;

  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, head, entries)
  {
    rc = group_add_regex(np->g, s, flags, err);
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
  int rc = 0;
  struct GroupNode *np = NULL;
  STAILQ_FOREACH(np, head, entries)
  {
    rc = group_remove_regex(np->g, s);
    if (empty_group(np->g))
      group_remove(np->g);
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
  for (struct Address *ap = g->as; ap; ap = ap->next)
    if (ap->mailbox && (mutt_str_strcasecmp(s, ap->mailbox) == 0))
      return true;

  return false;
}
