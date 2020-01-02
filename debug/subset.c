/**
 * @file
 * Dump all Config Subsets
 *
 * @authors
 * Copyright (C) 2019-2020 Richard Russon <rich@flatcap.org>
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
 * @page debug_subset Dump all Config Subsets
 *
 * Dump all Config Subsets
 */

#include "config.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "lib.h"

static const char *subset_get_scope(enum ConfigScope scope)
{
  switch (scope)
  {
    case SET_SCOPE_NEOMUTT:
      return "neomutt";
    case SET_SCOPE_ACCOUNT:
      return "account";
    case SET_SCOPE_MAILBOX:
      return "mailbox";
    default:
      return "unknown";
  }
}

void subset_dump(const struct ConfigSubset *sub)
{
  for (; sub; sub = sub->parent)
  {
    printf("%s: '%s' (%ld)", subset_get_scope(sub->scope), NONULL(sub->name),
           observer_count(sub->notify));
    if (sub->parent)
      printf(" --> ");
  }
  printf("\n");
}

void subset_dump_var2(const struct ConfigSubset *sub, const char *var)
{
  if (!sub)
    return;

  subset_dump_var2(sub->parent, var);
  if (sub->parent)
    printf(", ");

  struct HashElem *he = cs_subset_lookup(sub, var);
  if (he)
    printf("\033[1;32m");
  else
    printf("\033[1;31m");

  printf("%s:%s", NONULL(sub->name), var);

  printf("\033[0m");

  intptr_t value = cs_he_native_get(sub->cs, he, NULL);
  if (value == INT_MIN)
    printf("[X]");
  else if (DTYPE(he->type) != 0)
    printf("=%ld", value);
  else
    printf("(%ld)", value);
}

void subset_dump_var(const struct ConfigSubset *sub, const char *var)
{
  subset_dump_var2(sub, var);
  printf("\n");
}
