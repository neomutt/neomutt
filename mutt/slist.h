/**
 * @file
 * A separated list of strings
 *
 * @authors
 * Copyright (C) 2018-2019 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_LIB_SLIST_H
#define MUTT_LIB_SLIST_H

#include <stddef.h>
#include <stdbool.h>
#include "list.h"

#define SLIST_SEP_SPACE (1 << 13)
#define SLIST_SEP_COMMA (1 << 14)
#define SLIST_SEP_COLON (1 << 15)

#define SLIST_SEP_MASK  0xE000

#define SLIST_ALLOW_DUPES    (1 << 17)
#define SLIST_ALLOW_EMPTY    (1 << 18)
#define SLIST_CASE_SENSITIVE (1 << 19)

/**
 * struct Slist - String list
 */
struct Slist
{
  struct ListHead head;
  size_t count;
  unsigned int flags;
};

struct Slist *slist_add_list(struct Slist *list, const struct Slist *add);
struct Slist *slist_add_string(struct Slist *list, const char *str);
bool          slist_compare(const struct Slist *a, const struct Slist *b);
struct Slist *slist_dup(const struct Slist *list);
struct Slist *slist_empty(struct Slist **list);
void          slist_free(struct Slist **list);
bool          slist_is_member(const struct Slist *list, const char *str);
struct Slist *slist_parse(const char *str, int flags);
struct Slist *slist_remove_string(struct Slist *list, const char *str);

#endif /* MUTT_LIB_SLIST_H */
