/**
 * @file
 * Test code for mutt_hash_find_bucket()
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

void test_mutt_hash_find_bucket(void)
{
  // struct HashElem *mutt_hash_find_bucket(const struct HashTable *table, const char *strkey);

  int dummy1 = 42;
  int dummy2 = 13;
  int dummy3 = 99;

  {
    TEST_CHECK(!mutt_hash_find_bucket(NULL, "apple"));
  }

  {
    struct HashTable table = { 0 };
    TEST_CHECK(!mutt_hash_find_bucket(&table, NULL));
  }

  {
    struct HashTable *table = mutt_hash_new(128, MUTT_HASH_ALLOW_DUPS);
    mutt_hash_insert(table, "apple", &dummy1);
    mutt_hash_insert(table, "banana", &dummy1);
    mutt_hash_insert(table, "banana", &dummy2);
    mutt_hash_insert(table, "banana", &dummy3);
    mutt_hash_insert(table, "cherry", &dummy3);
    mutt_hash_find_bucket(table, "banana");
    mutt_hash_free(&table);
  }
}
