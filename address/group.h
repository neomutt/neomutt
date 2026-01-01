/**
 * @file
 * Handling for email address groups
 *
 * @authors
 * Copyright (C) 2017-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_ADDRESS_GROUP_H
#define MUTT_ADDRESS_GROUP_H

#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "address.h"

/**
 * struct Group - A set of email addresses
 */
struct Group
{
  struct AddressList al; ///< List of Addresses
  struct RegexList rs;   ///< Group Regex patterns
  char *name;            ///< Name of Group
};

/**
 * struct GroupNode - An element in a GroupList
 */
struct GroupNode
{
  struct Group *group;             ///< Address Group
  STAILQ_ENTRY(GroupNode) entries; ///< Linked list
};
STAILQ_HEAD(GroupList, GroupNode);

// Single Address Group
bool group_match(struct Group *g, const char *str);

// List of Addres Groups
void grouplist_add_group   (struct GroupList *gl, struct Group *g);
void grouplist_add_addrlist(struct GroupList *gl, struct AddressList *al);
int  grouplist_add_regex   (struct GroupList *gl, const char *str, uint16_t flags, struct Buffer *err);
void grouplist_destroy     (struct GroupList *gl);

// HashTable of Address Groups
struct HashTable *groups_new (void);
void              groups_free(struct HashTable **pptr);

struct Group *groups_get_group       (struct HashTable *groups, const char *name);
int           groups_remove_addrlist (struct HashTable *groups, struct GroupList *gl, struct AddressList *al);
void          groups_remove_grouplist(struct HashTable *groups, struct GroupList *gl);
int           groups_remove_regex    (struct HashTable *groups, struct GroupList *gl, const char *str);

#endif /* MUTT_ADDRESS_GROUP_H */
