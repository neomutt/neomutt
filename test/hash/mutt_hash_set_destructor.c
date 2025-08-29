/**
 * @file
 * Test code for mutt_hash_set_destructor()
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
#include <stdint.h>
#include "mutt/lib.h"

void dummy_hash_free(int type, void *obj, intptr_t data)
{
}

void test_mutt_hash_set_destructor(void)
{
  // void mutt_hash_set_destructor(struct HashTable *table, hash_hdata_free_t fn, intptr_t fn_data);

  {
    hash_hdata_free_t fn = dummy_hash_free;
    mutt_hash_set_destructor(NULL, fn, (intptr_t) "apple");
    TEST_CHECK_(1, "mutt_hash_set_destructor(NULL, fn, \"apple\")");
  }

  {
    struct HashTable table = { 0 };
    mutt_hash_set_destructor(&table, NULL, (intptr_t) "apple");
    TEST_CHECK_(1, "mutt_hash_set_destructor(&table, NULL, \"apple\")");
  }

  {
    struct HashTable table = { 0 };
    hash_hdata_free_t fn = dummy_hash_free;
    mutt_hash_set_destructor(&table, fn, 0);
    TEST_CHECK_(1, "mutt_hash_set_destructor(&table, fn, NULL)");
  }
}
