/**
 * @file
 * Test code for mutt_istr_find()
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

struct StriTest
{
  const char *str;
  size_t offset;
};

void test_mutt_istr_find(void)
{
  // const char *mutt_istr_find(const char *haystack, const char *needle);

  {
    TEST_CHECK(mutt_istr_find(NULL, "apple") == NULL);
  }

  {
    char *haystack = "apple";
    TEST_CHECK(mutt_istr_find(haystack, NULL) == haystack);
  }

  {
    TEST_CHECK(mutt_istr_find("apple", "banana") == NULL);
  }

  // clang-format off
  struct StriTest stri_tests[] =
  {
    { "appleTEXT",      0 },
    { "TEXTappleTEXT",  4 },
    { "TEXTapple",      4 },

    { "APpleTEXT",      0 },
    { "TEXTapPLeTEXT",  4 },
    { "TEXTAPPLE",      4 },

    { "TEXTappleapple", 4 },
    { "appleTEXTapple", 0 },
    { "appleappleTEXT", 0 },
  };
  // clang-format on

  {
    const char *find = "apple";

    for (size_t i = 0; i < mutt_array_size(stri_tests); i++)
    {
      struct StriTest *t = &stri_tests[i];
      TEST_CASE_("'%s'", t->str);

      const char *result = mutt_istr_find(t->str, find);
      TEST_CHECK(result == (t->str + t->offset));
    }
  }
}
