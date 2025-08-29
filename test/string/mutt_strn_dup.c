/**
 * @file
 * Test code for mutt_strn_dup()
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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
#include <string.h>
#include "mutt/lib.h"
#include "test_common.h"

void test_mutt_strn_dup(void)
{
  // char *mutt_strn_dup(const char *begin, size_t len);

  // clang-format off
  const char *str = "apple banana\0xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
  // clang-format on

  {
    TEST_CHECK(mutt_strn_dup(NULL, 7) == NULL);
  }

  {
    char *ptr = mutt_strn_dup(str, 7);
    TEST_CHECK(ptr != NULL);
    TEST_CHECK_STR_EQ(ptr, "apple b");
    TEST_MSG(ptr);
    FREE(&ptr);
  }

  {
    char *ptr = mutt_strn_dup(str + 3, 4);
    TEST_CHECK(ptr != NULL);
    TEST_CHECK_STR_EQ(ptr, "le b");
    TEST_MSG(ptr);
    FREE(&ptr);
  }

  {
    char *ptr = mutt_strn_dup(str + 3, 61);
    TEST_CHECK(ptr != NULL);
    TEST_CHECK_STR_EQ(ptr, "le banana");
    TEST_MSG(ptr);
    FREE(&ptr);
  }

  {
    char *ptr = mutt_strn_dup(str + 3, 0);
    TEST_CHECK(ptr != NULL);
    TEST_CHECK_STR_EQ(ptr, "");
    TEST_MSG(ptr);
    FREE(&ptr);
  }
}
