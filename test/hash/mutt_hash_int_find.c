/**
 * @file
 * Test code for mutt_hash_int_find()
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

void test_mutt_hash_int_find(void)
{
  // void *mutt_hash_int_find(const struct HashTable *table, unsigned int intkey);

  {
    mutt_hash_int_find(NULL, 0);
    TEST_CHECK_(1, "mutt_hash_int_find(NULL, 0)");
  }

  {
    struct HashTable *table = mutt_hash_int_new(128, MUTT_HASH_NO_FLAGS);
    mutt_hash_int_insert(table, 42, "apple");
    mutt_hash_int_insert(table, 42, "banana");
    mutt_hash_int_insert(table, 42 + 128, "cherry");
    mutt_hash_int_insert(table, 20 + 128, "damson");
    mutt_hash_int_insert(table, 20, "endive");
    mutt_hash_int_find(table, 42);
    mutt_hash_free(&table);
  }
}
