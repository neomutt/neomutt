/**
 * @file
 * Test code for mutt_str_inline_replace()
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

struct InlineReplaceTest
{
  const char *initial;
  int replace_len;
  const char *replace;
  const char *expected;
  bool success;
};

void test_mutt_str_inline_replace(void)
{
  // bool mutt_str_inline_replace(char *buf, size_t buflen, size_t xlen, const char *rstr);

  {
    TEST_CHECK(mutt_str_inline_replace(NULL, 10, 2, "apple") == false);
  }

  {
    char buf[32] = "banana";
    TEST_CHECK(mutt_str_inline_replace(buf, sizeof(buf), 2, NULL) == false);
  }

  // clang-format off
  struct InlineReplaceTest replace_tests[] =
  {
    { "XXXXbanana", 4, "",          "banana",        true,  },
    { "XXXXbanana", 4, "OO",        "OObanana",      true,  },
    { "XXXXbanana", 4, "OOOO",      "OOOObanana",    true,  },
    { "XXXXbanana", 4, "OOOOOO",    "OOOOOObanana",  true,  },
    { "XXXXbanana", 4, "OOOOOOO",   "OOOOOOObanana", true,  },
    { "XXXXbanana", 4, "OOOOOOOO",  "OOOOOOOObanan", false, },
    { "XXXXbanana", 4, "OOOOOOOOO", "OOOOOOOOObana", false, },
  };
  // clang-format on

  {
    char buf[14];
    for (size_t i = 0; i < mutt_array_size(replace_tests); i++)
    {
      struct InlineReplaceTest *t = &replace_tests[i];
      TEST_CASE_("'%s', %d, '%s'", t->initial, t->replace_len, t->replace);

      memset(buf, 0, sizeof(buf));

      mutt_str_copy(buf, t->initial, sizeof(buf));
      bool result = mutt_str_inline_replace(buf, sizeof(buf), t->replace_len, t->replace);
      TEST_CHECK(result == t->success);
      if (result)
        TEST_CHECK(strcmp(buf, t->expected) == 0);
      else
        TEST_CHECK(strcmp(buf, t->initial) == 0);
    }
  }
}
