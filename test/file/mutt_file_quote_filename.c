/**
 * @file
 * Test code for mutt_file_quote_filename()
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
#include "common.h"

void test_mutt_file_quote_filename(void)
{
  // size_t mutt_file_quote_filename(const char *filename, char *buf, size_t buflen);

  // clang-format off
  static struct TestValue tests[] = {
    { "plain",  "'plain'",       7 },
    { "ba`ck",  "'ba'\\`'ck'",  10 },
    { "qu'ote", "'qu'\\''ote'", 11 },
  };
  // clang-format on

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_file_quote_filename(NULL, buf, sizeof(buf)) == 0);
  }

  {
    TEST_CHECK(mutt_file_quote_filename("apple", NULL, 10) == 0);
  }

  int rc;
  char buf[256];
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    TEST_CASE(tests[i].first);
    memset(buf, 0, sizeof(buf));
    rc = mutt_file_quote_filename(tests[i].first, buf, sizeof(buf));
    if (!TEST_CHECK(rc == tests[i].retval) ||
        !TEST_CHECK(mutt_str_equal(buf, tests[i].second)))
    {
      TEST_MSG("Expected: %d", tests[i].retval);
      TEST_MSG("Actual:   %d", rc);
      TEST_MSG("Original: %s", tests[i].first);
      TEST_MSG("Expected: %s", tests[i].second);
      TEST_MSG("Actual:   %s", buf);
    }
  }
}
