/**
 * @file
 * Test code for mutt_str_substr_copy()
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

void test_mutt_str_substr_copy(void)
{
  // char *mutt_str_substr_copy(const char *begin, const char *end, char *buf, size_t buflen);

  // clang-format off
  const char *str = "apple banana\0xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
  // clang-format on

  {
    TEST_CHECK(mutt_str_substr_copy(str + 3, str + 7, NULL, 32) == NULL);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_str_substr_copy(NULL, str + 7, buf, sizeof(buf)) == buf);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_str_substr_copy(str + 3, NULL, buf, sizeof(buf)) == buf);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_str_substr_copy(str + 3, str + 7, buf, 0) == buf);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_str_substr_copy(str + 3, str + 3, buf, sizeof(buf)) == buf);
    TEST_CHECK(strcmp(buf, "") == 0);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_str_substr_copy(str + 3, str + 7, buf, sizeof(buf)) == buf);
    TEST_CHECK(strcmp(buf, "le b") == 0);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_str_substr_copy(str + 3, str + 64, buf, sizeof(buf)) == buf);
    TEST_CHECK(strcmp(buf, "le banana") == 0);
  }
}
