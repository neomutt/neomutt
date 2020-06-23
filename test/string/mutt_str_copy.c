/**
 * @file
 * Test code for mutt_str_copy()
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

void test_mutt_str_copy(void)
{
  // size_t mutt_str_copy(char *dest, const char *src, size_t dsize);

  {
    TEST_CHECK(mutt_str_copy(NULL, "apple", 10) == 0);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_str_copy(buf, NULL, sizeof(buf)) == 0);
  }

  char src[20] = "\0";
  char dst[10];

  { /* empty */
    size_t len = mutt_str_copy(dst, src, sizeof(dst));
    if (!TEST_CHECK(len == 0))
    {
      TEST_MSG("Expected: %zu", 0);
      TEST_MSG("Actual  : %zu", len);
    }
  }

  { /* normal */
    const char trial[] = "Hello";
    mutt_str_copy(src, trial, sizeof(src)); /* let's eat our own dogfood */
    size_t len = mutt_str_copy(dst, src, sizeof(dst));
    if (!TEST_CHECK(len == sizeof(trial) - 1))
    {
      TEST_MSG("Expected: %zu", sizeof(trial) - 1);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(dst, trial) == 0))
    {
      TEST_MSG("Expected: %s", trial);
      TEST_MSG("Actual  : %s", dst);
    }
  }

  { /* too long */
    const char trial[] = "Hello Hello Hello";
    mutt_str_copy(src, trial, sizeof(src));
    size_t len = mutt_str_copy(dst, src, sizeof(dst));
    if (!TEST_CHECK(len == sizeof(dst) - 1))
    {
      TEST_MSG("Expected: %zu", sizeof(dst) - 1);
      TEST_MSG("Actual  : %zu", len);
    }
  }
}
