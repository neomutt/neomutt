/**
 * @file
 * Handling for email address groups
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
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

#define MUTT_GROUP   0  ///< 'group' config command
#define MUTT_UNGROUP 1  ///< 'ungroup' config command

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

void mutt_grouplist_add            (struct GroupList *gl, struct Group *group);
void mutt_grouplist_add_addrlist   (struct GroupList *gl, struct AddressList *a);
int  mutt_grouplist_add_regex      (struct GroupList *gl, const char *s, uint16_t flags, struct Buffer *err);
void mutt_grouplist_cleanup        (void);
void mutt_grouplist_clear          (struct GroupList *gl);
void mutt_grouplist_destroy        (struct GroupList *gl);
void mutt_grouplist_init           (void);
int  mutt_grouplist_remove_addrlist(struct GroupList *gl, struct AddressList *a);
int  mutt_grouplist_remove_regex   (struct GroupList *gl, const char *s);

bool          mutt_group_match  (struct Group *g, const char *s);
struct Group *mutt_pattern_group(const char *pat);

#endif /* MUTT_ADDRESS_GROUP_H */
