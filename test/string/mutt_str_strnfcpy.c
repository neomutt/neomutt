/**
 * @file
 * Test code for mutt_str_strnfcpy()
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

void test_mutt_str_strnfcpy(void)
{
  // size_t mutt_str_strnfcpy(char *dest, const char *src, size_t n, size_t dsize);

  {
    TEST_CHECK(mutt_str_strnfcpy(NULL, "apple", 5, 10) == 0);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_str_strnfcpy(buf, NULL, 5, sizeof(buf)) == 0);
  }

  {
    TEST_CHECK(mutt_str_strnfcpy(NULL, NULL, 5, 10) == 0);
  }

  const char src[] = "One Two Three Four Five";
  char dst[10];
  char big[32];

  { /* copy a substring */
    size_t len = mutt_str_strnfcpy(dst, src, 3, sizeof(dst));
    if (!TEST_CHECK(len == 3))
    {
      TEST_MSG("Expected: %zu", 3);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(dst, "One") == 0))
    {
      TEST_MSG("Expected: %s", "One");
      TEST_MSG("Actual  : %s", dst);
    }
  }

  { /* try to copy the whole available length */
    size_t len = mutt_str_strnfcpy(dst, src, sizeof(dst), sizeof(dst));
    if (!TEST_CHECK(len == sizeof(dst) - 1))
    {
      TEST_MSG("Expected: %zu", sizeof(dst) - 1);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(dst, "One Two T") == 0))
    {
      TEST_MSG("Expected: %s", "One Two T");
      TEST_MSG("Actual  : %s", dst);
    }
  }

  { /* try to copy more than it fits */
    size_t len = mutt_str_strnfcpy(dst, src, strlen(src), sizeof(dst));
    if (!TEST_CHECK(len == sizeof(dst) - 1))
    {
      TEST_MSG("Expected: %zu", sizeof(dst) - 1);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(dst, "One Two T") == 0))
    {
      TEST_MSG("Expected: %s", "One Two T");
      TEST_MSG("Actual  : %s", dst);
    }
  }

  { /* try to copy more than available */
    size_t len = mutt_str_strnfcpy(big, src, sizeof(big), sizeof(big));
    if (!TEST_CHECK(len == sizeof(src) - 1))
    {
      TEST_MSG("Expected: %zu", sizeof(src) - 1);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(big, src) == 0))
    {
      TEST_MSG("Expected: %s", src);
      TEST_MSG("Actual  : %s", big);
    }
  }
}
