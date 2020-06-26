/**
 * @file
 * Test code for mutt_strn_cat()
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

void test_mutt_strn_cat(void)
{
  // char *mutt_strn_cat(char *d, size_t l, const char *s, size_t sl);

  {
    TEST_CHECK(mutt_strn_cat(NULL, 10, "apple", 5) == NULL);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_strn_cat(buf, 0, "apple", 5) == buf);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_strn_cat(buf, sizeof(buf), NULL, 5) == buf);
  }

  {
    TEST_CHECK(mutt_strn_cat(NULL, 10, NULL, 5) == NULL);
  }

  // Buffer adequate

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_strn_cat(buf, sizeof(buf), "", 1) == buf);
    TEST_CHECK(strcmp(buf, "") == 0);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_strn_cat(buf, sizeof(buf), "banana", 6) == buf);
    TEST_CHECK(strcmp(buf, "banana") == 0);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_strn_cat(buf, sizeof(buf), "banana", 3) == buf);
    TEST_CHECK(strcmp(buf, "ban") == 0);
  }

  {
    char buf[32] = "apple";
    TEST_CHECK(mutt_strn_cat(buf, sizeof(buf), "", 1) == buf);
    TEST_CHECK(strcmp(buf, "apple") == 0);
  }

  {
    char buf[32] = "apple";
    TEST_CHECK(mutt_strn_cat(buf, sizeof(buf), "banana", 6) == buf);
    TEST_CHECK(strcmp(buf, "applebanana") == 0);
  }

  {
    char buf[32] = "apple";
    TEST_CHECK(mutt_strn_cat(buf, sizeof(buf), "banana", 3) == buf);
    TEST_CHECK(strcmp(buf, "appleban") == 0);
  }

  // Buffer too small

  {
    char buf[6] = { 0 };
    TEST_CHECK(mutt_strn_cat(buf, sizeof(buf), "banana", 6) == buf);
    TEST_CHECK(strcmp(buf, "banan") == 0);
  }

  {
    char buf[8] = "apple";
    TEST_CHECK(mutt_strn_cat(buf, sizeof(buf), "banana", 6) == buf);
    TEST_CHECK(strcmp(buf, "appleba") == 0);
  }
}
