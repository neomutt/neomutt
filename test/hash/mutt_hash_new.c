/**
 * @file
 * Test code for mutt_hash_new()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include "mutt/lib.h"

void test_mutt_hash_new(void)
{
  // struct HashTable *mutt_hash_new(size_t num_elems, HashFlags flags);

  int dummy1 = 42;
  int dummy2 = 13;
  int dummy3 = 99;

  {
    struct HashTable *table = mutt_hash_new(0, MUTT_HASH_NO_FLAGS);
    mutt_hash_free(&table);
  }

  {
    struct HashTable *table = mutt_hash_new(32, MUTT_HASH_STRCASECMP);
    char buf[32];
    for (size_t i = 0; i < 50; i++)
    {
      snprintf(buf, sizeof(buf), "apple%ld", i);
      mutt_hash_insert(table, strdup(buf), &dummy1);
    }
    mutt_hash_free(&table);
  }

  {
    struct HashTable *table = mutt_hash_new(128, MUTT_HASH_STRDUP_KEYS);
    mutt_hash_insert(table, "apple", &dummy1);
    mutt_hash_insert(table, "banana", &dummy2);
    mutt_hash_insert(table, "cherry", &dummy3);
    mutt_hash_free(&table);
  }

  {
    struct HashTable *table = mutt_hash_new(128, MUTT_HASH_ALLOW_DUPS);
    mutt_hash_insert(table, "apple", &dummy1);
    mutt_hash_insert(table, "apple", &dummy2);
    mutt_hash_insert(table, "apple", &dummy3);
    mutt_hash_free(&table);
  }
}
