/**
 * @file
 * Test code for mutt_hash_find()
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stddef.h>
#include "mutt/lib.h"

void test_mutt_hash_find(void)
{
  // void *mutt_hash_find(const struct HashTable *table, const char *strkey);
  // void *mutt_hash_find_n(const struct HashTable *table, const char *strkey, int keylen);

  int dummy1 = 42;
  int dummy2 = 13;
  int dummy3 = 99;
  int dummy4 = 72;

  {
    TEST_CHECK(!mutt_hash_find(NULL, "apple"));
  }

  {
    struct HashTable *table = mutt_hash_new(10, MUTT_HASH_NO_FLAGS);
    TEST_CHECK(!mutt_hash_find(table, "apple"));
    mutt_hash_free(&table);
  }

  {
    struct HashTable *table = mutt_hash_new(10, MUTT_HASH_NO_FLAGS);
    mutt_hash_insert(table, "apple", &dummy1);
    mutt_hash_insert(table, "banana", &dummy2);
    mutt_hash_insert(table, "cherry", &dummy3);
    TEST_CHECK(mutt_hash_find(table, "apple") != NULL);
    mutt_hash_free(&table);
  }

  {
    TEST_CHECK(!mutt_hash_find_n(NULL, "apple", 5));
  }

  {
    struct HashTable *table = mutt_hash_new(10, MUTT_HASH_NO_FLAGS);
    TEST_CHECK(!mutt_hash_find_n(table, "apple", 5));
    mutt_hash_free(&table);
  }

  {
    struct HashTable *table = mutt_hash_new(10, MUTT_HASH_NO_FLAGS);
    mutt_hash_insert(table, "apple", &dummy1);
    mutt_hash_insert(table, "banana", &dummy2);
    mutt_hash_insert(table, "cherry", &dummy3);
    TEST_CHECK(mutt_hash_find_n(table, "apple", 5) != NULL);
    mutt_hash_free(&table);
  }

  {
    struct HashTable *table = mutt_hash_new(10, MUTT_HASH_NO_FLAGS);
    mutt_hash_insert(table, "apple", &dummy1);
    mutt_hash_insert(table, "banana", &dummy2);
    mutt_hash_insert_n(table, "cherry", 6, &dummy3);
    mutt_hash_insert_n(table, "damson", 6, &dummy4);

    TEST_CHECK(mutt_hash_find(table, "apple") != NULL);
    TEST_CHECK(mutt_hash_find_n(table, "apple", 5) != NULL);

    TEST_CHECK(!mutt_hash_find(table, "applecart"));
    TEST_CHECK(!mutt_hash_find_n(table, "applecart", 9));

    TEST_CHECK(mutt_hash_find(table, "banana") != NULL);
    TEST_CHECK(mutt_hash_find_n(table, "banana", 6) != NULL);

    TEST_CHECK(!mutt_hash_find(table, "banananana"));
    TEST_CHECK(!mutt_hash_find_n(table, "banananana", 10));

    TEST_CHECK(mutt_hash_find(table, "cherry") != NULL);
    TEST_CHECK(mutt_hash_find_n(table, "cherry", 6) != NULL);

    TEST_CHECK(!mutt_hash_find(table, "cher"));
    TEST_CHECK(!mutt_hash_find_n(table, "cher", 4));

    TEST_CHECK(mutt_hash_find(table, "damson") != NULL);
    TEST_CHECK(mutt_hash_find_n(table, "damson", 6) != NULL);

    TEST_CHECK(!mutt_hash_find(table, "dam"));
    TEST_CHECK(!mutt_hash_find_n(table, "dam", 3));

    mutt_hash_free(&table);
  }

  {
    struct HashTable *table = mutt_hash_new(10, MUTT_HASH_STRCASECMP);
    mutt_hash_insert(table, "apple", &dummy1);
    mutt_hash_insert(table, "banana", &dummy2);
    mutt_hash_insert_n(table, "cherry", 6, &dummy3);
    mutt_hash_insert_n(table, "damson", 6, &dummy4);

    TEST_CHECK(mutt_hash_find(table, "APPLE") != NULL);
    TEST_CHECK(mutt_hash_find_n(table, "APPLE", 5) != NULL);

    TEST_CHECK(!mutt_hash_find(table, "APPLECART"));
    TEST_CHECK(!mutt_hash_find_n(table, "APPLECARt", 9));

    TEST_CHECK(mutt_hash_find(table, "BANANA") != NULL);
    TEST_CHECK(mutt_hash_find_n(table, "BANANA", 6) != NULL);

    TEST_CHECK(!mutt_hash_find(table, "BANANANANA"));
    TEST_CHECK(!mutt_hash_find_n(table, "BANANANANA", 10));

    TEST_CHECK(mutt_hash_find(table, "CHERRY") != NULL);
    TEST_CHECK(mutt_hash_find_n(table, "CHERRY", 6) != NULL);

    TEST_CHECK(!mutt_hash_find(table, "CHER"));
    TEST_CHECK(!mutt_hash_find_n(table, "CHER", 4));

    TEST_CHECK(mutt_hash_find(table, "DAMSON") != NULL);
    TEST_CHECK(mutt_hash_find_n(table, "DAMSON", 6) != NULL);

    TEST_CHECK(!mutt_hash_find(table, "DAM"));
    TEST_CHECK(!mutt_hash_find_n(table, "DAM", 3));

    mutt_hash_free(&table);
  }
}
