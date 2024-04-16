/**
 * @file
 * Test Expando Format
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
#include <limits.h>
#include <string.h>
#include "mutt/lib.h"
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep

struct ExpandoFormat *parse_format(const char *start, const char *end,
                                   struct ExpandoParseError *error);

struct TestCase
{
  const char *src;
  char leader;
  int min_cols;
  int max_cols;
  enum FormatJustify justify;
};

void test_expando_node_expando_format(void)
{
  static const struct TestCase tests[] = {
    // clang-format off
    { "5x",    ' ', 5, INT_MAX, JUSTIFY_RIGHT  },
    { ".7x",   ' ', 0, 7,       JUSTIFY_RIGHT  },
    { "5.7x",  ' ', 5, 7,       JUSTIFY_RIGHT  },
    { "-5x",   ' ', 5, INT_MAX, JUSTIFY_LEFT   },
    { "-.7x",  ' ', 0, 7,       JUSTIFY_LEFT   },
    { "-5.7x", ' ', 5, 7,       JUSTIFY_LEFT   },
    { "05x",   '0', 5, INT_MAX, JUSTIFY_RIGHT  },
    { "=5x",   ' ', 5, INT_MAX, JUSTIFY_CENTER },
    // clang-format on
  };

  // struct ExpandoFormat *parse_format(const char *start, const char *end, struct ExpandoParseError *error);
  {
    struct ExpandoParseError err = { 0 };

    for (size_t i = 0; i < mutt_array_size(tests); i++)
    {
      const char *start = tests[i].src;
      const char *end = strchr(start, 'x');

      TEST_CASE(start);

      struct ExpandoFormat *fmt = parse_format(start, end, &err);
      TEST_CHECK(fmt != NULL);
      TEST_CHECK(fmt->leader == tests[i].leader);
      TEST_CHECK(fmt->min_cols == tests[i].min_cols);
      TEST_CHECK(fmt->max_cols == tests[i].max_cols);
      TEST_CHECK(fmt->justification == tests[i].justify);
      FREE(&fmt);
    }
  }

  // degenerate tests
  {
    struct ExpandoParseError err = { 0 };
    struct ExpandoFormat *fmt = NULL;
    const char *start = NULL;
    const char *end = NULL;

    start = "99999x";
    end = strchr(start, 'x');
    fmt = parse_format(start, end, &err);
    TEST_CHECK(fmt == NULL);

    start = "4.x";
    end = strchr(start, 'x');
    fmt = parse_format(start, end, &err);
    TEST_CHECK(fmt == NULL);

    start = "4.99999x";
    end = strchr(start, 'x');
    fmt = parse_format(start, end, &err);
    TEST_CHECK(fmt == NULL);

    start = "4.99999x";
    fmt = parse_format(start, start, &err);
    TEST_CHECK(fmt == NULL);
  }
}
