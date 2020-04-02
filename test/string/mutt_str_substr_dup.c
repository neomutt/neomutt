/**
 * @file
 * Test code for mutt_str_substr_dup()
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

void test_mutt_str_substr_dup(void)
{
  // char *mutt_str_substr_dup(const char *begin, const char *end);

  // clang-format off
  const char *str = "apple banana\0xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
  // clang-format on

  {
    TEST_CHECK(mutt_str_substr_dup(NULL, str + 7) == NULL);
  }

  {
    char *ptr = mutt_str_substr_dup(str, str + 7);
    TEST_CHECK(ptr != NULL);
    TEST_CHECK(strcmp(ptr, "apple b") == 0);
    TEST_MSG(ptr);
    FREE(&ptr);
  }

  {
    char *ptr = mutt_str_substr_dup(str + 3, str + 7);
    TEST_CHECK(ptr != NULL);
    TEST_CHECK(strcmp(ptr, "le b") == 0);
    TEST_MSG(ptr);
    FREE(&ptr);
  }

  {
    char *ptr = mutt_str_substr_dup(str + 3, str + 64);
    TEST_CHECK(ptr != NULL);
    TEST_CHECK(strcmp(ptr, "le banana") == 0);
    TEST_MSG(ptr);
    FREE(&ptr);
  }

  {
    char *ptr = mutt_str_substr_dup(str + 3, NULL);
    TEST_CHECK(ptr != NULL);
    TEST_CHECK(strcmp(ptr, "le banana") == 0);
    TEST_MSG(ptr);
    FREE(&ptr);
  }

  {
    char *ptr = mutt_str_substr_dup(str + 7, str + 3);
    TEST_CHECK(ptr == NULL);
  }
}
