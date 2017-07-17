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

#ifndef _MUTT_GROUP_H
#define _MUTT_GROUP_H

#include <stdbool.h>

struct Address;
struct Buffer;

#define MUTT_GROUP   0
#define MUTT_UNGROUP 1

/**
 * struct Group - A set of email addresses
 */
struct Group
{
  struct Address *as;
  struct RxList *rs;
  char *name;
};

/**
 * struct GroupContext - A set of Groups
 */
struct GroupContext
{
  struct Group *g;
  struct GroupContext *next;
};

void mutt_group_context_add(struct GroupContext **ctx, struct Group *group);
void mutt_group_context_destroy(struct GroupContext **ctx);
void mutt_group_context_add_adrlist(struct GroupContext *ctx, struct Address *a);
int mutt_group_context_add_rx(struct GroupContext *ctx, const char *s, int flags, struct Buffer *err);

bool mutt_group_match(struct Group *g, const char *s);

int mutt_group_context_clear(struct GroupContext **ctx);
int mutt_group_context_remove_rx(struct GroupContext *ctx, const char *s);
int mutt_group_context_remove_adrlist(struct GroupContext *ctx, struct Address *a);

struct Group *mutt_pattern_group(const char *k);

#endif /* _MUTT_GROUP_H */
