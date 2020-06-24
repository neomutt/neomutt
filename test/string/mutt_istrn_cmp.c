/**
 * @file
 * Test code for mutt_istrn_cmp()
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

void test_mutt_istrn_cmp(void)
{
  // int mutt_istrn_cmp(const char *a, const char *b, size_t l);

  TEST_CHECK(mutt_istrn_cmp(NULL, "apple", 3) != 0);
  TEST_CHECK(mutt_istrn_cmp("apple", NULL, 3) != 0);
  TEST_CHECK(mutt_istrn_cmp(NULL, NULL, 3) == 0);
  TEST_CHECK(mutt_istrn_cmp("", "", 3) == 0);

  TEST_CHECK(mutt_istrn_cmp("apple", "apple", 5) == 0);
  TEST_CHECK(mutt_istrn_cmp("apple", "APPLE", 5) == 0);

  TEST_CHECK(mutt_istrn_cmp("apple", "apple2", 10) != 0);
  TEST_CHECK(mutt_istrn_cmp("apple1", "apple", 10) != 0);

  TEST_CHECK(mutt_istrn_cmp("apple", "apple2", 5) == 0);
  TEST_CHECK(mutt_istrn_cmp("apple1", "apple", 5) == 0);

  TEST_CHECK(mutt_istrn_cmp("apple", "banana", 5) < 0);
  TEST_CHECK(mutt_istrn_cmp("banana", "apple", 5) > 0);

  TEST_CHECK(mutt_istrn_cmp("applea", "appleb", 6) < 0);
  TEST_CHECK(mutt_istrn_cmp("appleb", "applea", 6) > 0);

  TEST_CHECK(mutt_istrn_cmp("a", "aa", 10) < 0);
  TEST_CHECK(mutt_istrn_cmp("aa", "a", 10) > 0);
}
