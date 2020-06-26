/**
 * @file
 * Test code for mutt_strn_copy()
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

void test_mutt_strn_copy(void)
{
  // char *mutt_strn_copy(char *dest, const char *begin, const char *end, size_t dsize);

  // clang-format off
  const char *str = "apple banana\0xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
  // clang-format on

  {
    TEST_CHECK(mutt_strn_copy(NULL, str + 3, 4, 32) == NULL);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_strn_copy(buf, NULL, 7, sizeof(buf)) == buf);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_strn_copy(buf, str + 3, 0, sizeof(buf)) == buf);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_strn_copy(buf, str + 3, 4, 0) == buf);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_strn_copy(buf, str + 3, 0, sizeof(buf)) == buf);
    TEST_CHECK(strcmp(buf, "") == 0);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_strn_copy(buf, str + 3, 4, sizeof(buf)) == buf);
    TEST_CHECK(strcmp(buf, "le b") == 0);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_strn_copy(buf, str + 3, 61, sizeof(buf)) == buf);
    TEST_CHECK(strcmp(buf, "le banana") == 0);
  }
}
