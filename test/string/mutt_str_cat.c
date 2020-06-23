/**
 * @file
 * Test code for mutt_str_cat()
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

void test_mutt_str_cat(void)
{
  // char *mutt_str_cat(char *buf, size_t buflen, const char *s);

  {
    TEST_CHECK(mutt_str_cat(NULL, 10, "apple") == NULL);
  }

  {
    char buf[64] = { 0 };
    TEST_CHECK(mutt_str_cat(buf, 0, "apple") == buf);
    TEST_CHECK(strcmp(buf, "") == 0);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_str_cat(buf, sizeof(buf), NULL) == buf);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_str_cat(buf, sizeof(buf), "") == buf);
    TEST_CHECK(strcmp(buf, "") == 0);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_str_cat(buf, sizeof(buf), "banana") == buf);
    TEST_CHECK(strcmp(buf, "banana") == 0);
  }

  {
    char buf[32] = "apple";
    TEST_CHECK(mutt_str_cat(buf, sizeof(buf), "") == buf);
    TEST_CHECK(strcmp(buf, "apple") == 0);
  }

  {
    char buf[32] = "apple";
    TEST_CHECK(mutt_str_cat(buf, sizeof(buf), "banana") == buf);
    TEST_CHECK(strcmp(buf, "applebanana") == 0);
  }
}
