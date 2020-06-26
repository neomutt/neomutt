/**
 * @file
 * Test code for mutt_strn_equal()
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

void test_mutt_strn_equal(void)
{
  // bool mutt_strn_equal(const char *a, const char *b, size_t l);

  TEST_CHECK(!mutt_strn_equal(NULL, "apple", 5));
  TEST_CHECK(!mutt_strn_equal("apple", NULL, 5));
  TEST_CHECK(mutt_strn_equal(NULL, NULL, 5));

  TEST_CHECK(mutt_strn_equal("", "", 1));
  TEST_CHECK(mutt_strn_equal("apple", "apple", 5));
  TEST_CHECK(!mutt_strn_equal("apple", "APPLE", 5));
  TEST_CHECK(!mutt_strn_equal("apple", "apple2", 6));
  TEST_CHECK(!mutt_strn_equal("apple1", "apple", 6));
  TEST_CHECK(mutt_strn_equal("apple", "apple2", 5));
  TEST_CHECK(mutt_strn_equal("apple1", "apple", 5));
}
