/**
 * @file
 * Test code for mutt_istrn_rfind()
 *
 * @authors
 * Copyright (C) 2021 Austin Ray <austin@austinray.io>
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
#include <stddef.h>
#include "mutt/lib.h"

struct RistrnTest
{
  const char *str;
  size_t len;
  size_t offset;
};

void test_mutt_istrn_rfind(void)
{
  // const char *mutt_istrn_rfind(const char *haystack, size_t haystack_length, const char *needle);

  {
    TEST_CHECK(mutt_istrn_rfind(NULL, 10, "apple") == NULL);
    TEST_CHECK(mutt_istrn_rfind("apple", 0, "apple") == NULL);
    TEST_CHECK(mutt_istrn_rfind("apple", 10, NULL) == NULL);
    TEST_CHECK(mutt_istrn_rfind("", 1, "apple") == NULL);
    TEST_CHECK(mutt_istrn_rfind("text", 1, "apple") == NULL);
    TEST_CHECK(mutt_istrn_rfind("textapple", 8, "apple") == NULL);
  }

  // clang-format off
  struct RistrnTest ristrn_tests[] =
  {
    { "AppleTEXT",      9,  0 },
    { "TEXTaPpleTEXT",  13, 4 },
    { "TEXTapPle",      9,  4 },

    { "TEXTapPleappLe", 14, 9 },
    { "appLeTEXTapplE", 14, 9 },
    { "appleAPPLETEXT", 14, 5 },
  };
  // clang-format on

  {
    const char *find = "apple";

    for (size_t i = 0; i < mutt_array_size(ristrn_tests); i++)
    {
      struct RistrnTest *t = &ristrn_tests[i];
      TEST_CASE_("'%s'", t->str);

      const char *result = mutt_istrn_rfind(t->str, t->len, find);
      TEST_CHECK(result == (t->str + t->offset));
    }
  }
}
