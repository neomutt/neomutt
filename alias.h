/**
 * @file
 * Representation of a single alias to an email address
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_ALIAS_H
#define MUTT_ALIAS_H

#include <stddef.h>
#include <stdbool.h>
#include "mutt/mutt.h"
#include "address/address.h"

struct Envelope;

/**
 * struct Alias - A shortcut for an email address
 */
struct Alias
{
  char *name;
  struct AddressList addr;
  bool tagged;
  bool del;
  short num;
  TAILQ_ENTRY(Alias) entries;
};
TAILQ_HEAD(AliasList, Alias);

struct Alias   *mutt_alias_new(void);
void            mutt_alias_free(struct Alias **p);
void            mutt_alias_create(struct Envelope *cur, struct AddressList *al);
void            mutt_aliaslist_free(struct AliasList *a_list);
struct AddressList *mutt_alias_lookup(const char *s);
void            mutt_expand_aliases_env(struct Envelope *env);
void            mutt_expand_aliases(struct AddressList *al);
struct AddressList *mutt_get_address(struct Envelope *env, const char **pfxp);

bool mutt_addr_is_user(const struct Address *addr);
int mutt_alias_complete(char *buf, size_t buflen);
void mutt_alias_add_reverse(struct Alias *t);
void mutt_alias_delete_reverse(struct Alias *t);
struct Address *mutt_alias_reverse_lookup(const struct Address *a);

#endif /* MUTT_ALIAS_H */
