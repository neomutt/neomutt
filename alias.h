/**
 * @file
 * Representation of a single alias to an email address
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _MUTT_ALIAS_H
#define _MUTT_ALIAS_H

#include <stdbool.h>

struct Envelope;
struct Address;

/**
 * struct Alias - A shortcut for an email address
 */
struct Alias
{
  char *name;
  struct Address *addr;
  struct Alias *next;
  bool tagged;
  bool del;
  short num;
};

void            mutt_alias_create(struct Envelope *cur, struct Address *iaddr);
void            mutt_alias_free(struct Alias **p);
struct Address *mutt_alias_lookup(const char *s);

struct Address *mutt_expand_aliases(struct Address *a);
void mutt_expand_aliases_env(struct Envelope *env);
struct Address *mutt_get_address(struct Envelope *env, char **pfxp);

#endif /* _MUTT_ALIAS_H */
