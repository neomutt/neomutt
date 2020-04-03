/**
 * @file
 * Test code for mutt_str_append_item()
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

struct AppendTest
{
  const char *first;
  const char *second;
  char sep;
  const char *result;
  size_t res_len;
};

void test_mutt_str_append_item(void)
{
  // void mutt_str_append_item(char **str, const char *item, int sep);

  {
    mutt_str_append_item(NULL, "apple", ',');
    TEST_CHECK_(1, "mutt_str_append_item(NULL, \"apple\", ',')");
  }

  {
    char *ptr = NULL;
    mutt_str_append_item(&ptr, NULL, ',');
    TEST_CHECK_(1, "mutt_str_append_item(&ptr, NULL, ',')");
  }

  // clang-format off
  struct AppendTest append_tests[] =
  {
    { NULL,    "banana", '/',  "banana"       },
    { "",      "banana", '/',  "banana"       },
    { "apple", "banana", '/',  "apple/banana" },
    { NULL,    "banana", '\0', "banana"       },
    { "",      "banana", '\0', "banana"       },
    { "apple", "banana", '\0', "applebanana"  },
  };
  // clang-format on

  {
    for (size_t i = 0; i < mutt_array_size(append_tests); i++)
    {
      struct AppendTest *t = &append_tests[i];
      char *str = t->first ? strdup(t->first) : NULL;
      TEST_CASE_("\"%s\", \"%s\", '%c'", NONULL(t->first), t->second, t->sep);
      mutt_str_append_item(&str, t->second, t->sep);
      TEST_CHECK(strcmp(str, t->result) == 0);
      FREE(&str);
    }
  }
}
