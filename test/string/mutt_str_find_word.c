/**
 * @file
 * Test code for mutt_str_find_word()
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

struct FindWordTest
{
  const char *str;
  size_t offset;
};

void test_mutt_str_find_word(void)
{
  // const char *mutt_str_find_word(const char *src);

  {
    TEST_CHECK(mutt_str_find_word(NULL) == NULL);
  }

  // clang-format off
  struct FindWordTest find_tests[] =
  {
    { "apple banana",    5 },
    { "apple\tbanana",   5 },
    { "apple\nbanana",   5 },

    { "apple\t banana",  5 },
    { "apple\n\nbanana", 5 },
    { "apple   banana",  5 },

    { "\t banana",       8 },
    { "\n\nbanana",      8 },
    { "   banana",       9 },

    { " \t\n ",          4 },
  };
  // clang-format on

  {
    for (size_t i = 0; i < mutt_array_size(find_tests); i++)
    {
      struct FindWordTest *t = &find_tests[i];

      TEST_CASE_("'%s'", t->str);
      const char *result = mutt_str_find_word(t->str);
      TEST_CHECK(result == (t->str + t->offset));
    }
  }
}
