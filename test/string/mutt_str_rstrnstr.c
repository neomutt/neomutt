/**
 * @file
 * Test code for mutt_str_rstrnstr()
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

struct RstrnTest
{
  const char *str;
  size_t len;
  size_t offset;
};

void test_mutt_str_rstrnstr(void)
{
  // const char *mutt_str_rstrnstr(const char *haystack, size_t haystack_length, const char *needle);

  {
    TEST_CHECK(mutt_str_rstrnstr(NULL, 10, "apple") == NULL);
    TEST_CHECK(mutt_str_rstrnstr("apple", 0, "apple") == NULL);
    TEST_CHECK(mutt_str_rstrnstr("apple", 10, NULL) == NULL);
    TEST_CHECK(mutt_str_rstrnstr("", 1, "apple") == NULL);
    TEST_CHECK(mutt_str_rstrnstr("text", 1, "apple") == NULL);
    TEST_CHECK(mutt_str_rstrnstr("textapple", 8, "apple") == NULL);
  }

  // clang-format off
  struct RstrnTest rstrn_tests[] =
  {
    { "appleTEXT",      9,  0 },
    { "TEXTappleTEXT",  13, 4 },
    { "TEXTapple",      9,  4 },

    { "TEXTappleapple", 14, 9 },
    { "appleTEXTapple", 14, 9 },
    { "appleappleTEXT", 14, 5 },
  };
  // clang-format on

  {
    const char *find = "apple";

    for (size_t i = 0; i < mutt_array_size(rstrn_tests); i++)
    {
      struct RstrnTest *t = &rstrn_tests[i];
      TEST_CASE_("'%s'", t->str);

      const char *result = mutt_str_rstrnstr(t->str, t->len, find);
      TEST_CHECK(result == (t->str + t->offset));
    }
  }
}
