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
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep

struct TestCase
{
  const char *src;
  char leader;
  int min_cols;
  int max_cols;
  bool lower;
  enum FormatJustify justify;
};

void test_expando_node_expando_format(void)
{
  static const struct TestCase tests[] = {
    // clang-format off
    { "5x",      ' ', 5,  -1, false, JUSTIFY_RIGHT  },
    { ".7x",     '0', 0,  7,  false, JUSTIFY_RIGHT  },
    { "5.7x",    '0', 5,  7,  false, JUSTIFY_RIGHT  },
    { "-5x",     ' ', 5,  -1, false, JUSTIFY_LEFT   },
    { "-.7x",    '0', 0,  7,  false, JUSTIFY_LEFT   },
    { "-5.7x",   '0', 5,  7,  false, JUSTIFY_LEFT   },
    { "05x",     '0', 5,  -1, false, JUSTIFY_RIGHT  },
    { "=5x",     ' ', 5,  -1, false, JUSTIFY_CENTER },
    { "_x",      ' ', 0,  -1, true,  JUSTIFY_RIGHT  },
    { "5_x",     ' ', 5,  -1, true,  JUSTIFY_RIGHT  },
    { ".7_x",    '0', 0,  7,  true,  JUSTIFY_RIGHT  },
    { "5.7_x",   '0', 5,  7,  true,  JUSTIFY_RIGHT  },
    { "-5_x",    ' ', 5,  -1, true,  JUSTIFY_LEFT   },
    { "-.7_x",   '0', 0,  7,  true,  JUSTIFY_LEFT   },
    { "-5.7_x",  '0', 5,  7,  true,  JUSTIFY_LEFT   },

    { "5x",      ' ', 5,  -1, false, JUSTIFY_RIGHT  },
    { "05x",     '0', 5,  -1, false, JUSTIFY_RIGHT  },
    { "-5x",     ' ', 5,  -1, false, JUSTIFY_LEFT   },

    { ".8x",     '0', 0,  8,  false, JUSTIFY_RIGHT  },
    { "5.8x",    '0', 5,  8,  false, JUSTIFY_RIGHT  },
    { "-5.8x",   '0', 5,  8,  false, JUSTIFY_LEFT   },

    { "12.8x",   '0', 12,  8, false, JUSTIFY_RIGHT  },
    { "-12.8x",  '0', 12,  8, false, JUSTIFY_LEFT   },

    { "=12.8x",  '0', 12,  8, false, JUSTIFY_CENTER },

    { "-.8x",    '0', 0,  8,  false, JUSTIFY_LEFT   },
    { "5.x",     ' ', 5,  0,  false, JUSTIFY_RIGHT  },
    { "-5.x",    ' ', 5,  0,  false, JUSTIFY_LEFT   },

    { "08x",     '0', 8,  -1, false, JUSTIFY_RIGHT  },
    { "8x",      ' ', 8,  -1, false, JUSTIFY_RIGHT  },
    { "-8x",     ' ', 8,  -1, false, JUSTIFY_LEFT   },

    { "-05x",    ' ', 5,  -1, false, JUSTIFY_LEFT   },
    { "-08x",    ' ', 8,  -1, false, JUSTIFY_LEFT   },

    { "0.8x",    '0', 0,  8,  false, JUSTIFY_RIGHT  },
    { "05.8x",   '0', 5,  8,  false, JUSTIFY_RIGHT  },
    { "05.x",    ' ', 5,  0,  false, JUSTIFY_RIGHT  },
    { "0.x",     ' ', 0,  0,  false, JUSTIFY_RIGHT  },

    { "-0.8x",   '0', 0,  8,  false, JUSTIFY_LEFT   },
    { "-05.8x",  '0', 5,  8,  false, JUSTIFY_LEFT   },
    { "-05.x",   ' ', 5,  0,  false, JUSTIFY_LEFT   },
    { "-0.x",    ' ', 0,  0,  false, JUSTIFY_LEFT   },

    { "5.0x",    ' ', 5,  0,  false, JUSTIFY_RIGHT  },
    { ".0x",     ' ', 0,  0,  false, JUSTIFY_RIGHT  },
    { "-5.0x",   ' ', 5,  0,  false, JUSTIFY_LEFT   },
    { "-.0x",    ' ', 0,  0,  false, JUSTIFY_LEFT   },

    { "05.0x",   ' ', 5,  0,  false, JUSTIFY_RIGHT  },
    { "0.0x",    ' ', 0,  0,  false, JUSTIFY_RIGHT  },

    { "-05.0x",  ' ', 5,  0,  false, JUSTIFY_LEFT   },
    { "-0.0x",   ' ', 0,  0,  false, JUSTIFY_LEFT   },

    { "04x",     '0', 4,  -1, false, JUSTIFY_RIGHT  },
    { "4x",      ' ', 4,  -1, false, JUSTIFY_RIGHT  },
    { "14x",     ' ', 14, -1, false, JUSTIFY_RIGHT  },
    { ".0x",     ' ', 0,   0, false, JUSTIFY_RIGHT  },
    { ".4x",     '0', 0,   4, false, JUSTIFY_RIGHT  },
    { ".14x",    '0', 0,  14, false, JUSTIFY_RIGHT  },
    { "20.0x",   ' ', 20,  0, false, JUSTIFY_RIGHT  },
    { "20.4x",   '0', 20,  4, false, JUSTIFY_RIGHT  },
    { "20.14x",  '0', 20, 14, false, JUSTIFY_RIGHT  },
    // clang-format on
  };

  // degenerate tests
  {
    struct ExpandoParseError err = { 0 };
    struct ExpandoFormat *fmt = NULL;
    const char *str = NULL;
    const char *parsed_until = NULL;

    TEST_CASE("degenerate");

    str = NULL;
    fmt = parse_format(str, &parsed_until, &err);
    TEST_CHECK(fmt == NULL);

    str = "";
    fmt = parse_format(str, &parsed_until, &err);
    TEST_CHECK(fmt == NULL);

    str = "99999x";
    fmt = parse_format(str, &parsed_until, &err);
    TEST_CHECK(fmt == NULL);

    str = "4.99999x";
    fmt = parse_format(str, &parsed_until, &err);
    TEST_CHECK(fmt == NULL);

    str = "4.99999x";
    fmt = parse_format(str, &parsed_until, &err);
    TEST_CHECK(fmt == NULL);
  }

  // struct ExpandoFormat *parse_format(const char *str, const char **parsed_until, struct ExpandoParseError *err);
  {
    struct ExpandoParseError err = { 0 };

    for (size_t i = 0; i < mutt_array_size(tests); i++)
    {
      const char *str = tests[i].src;
      const char *parsed_until = NULL;

      TEST_CASE(str);

      struct ExpandoFormat *fmt = parse_format(str, &parsed_until, &err);
      TEST_CHECK(fmt != NULL);
      TEST_CHECK(fmt->leader == tests[i].leader);
      TEST_CHECK(fmt->min_cols == tests[i].min_cols);
      TEST_CHECK(fmt->max_cols == tests[i].max_cols);
      TEST_CHECK(fmt->justification == tests[i].justify);
      TEST_CHECK(fmt->lower == tests[i].lower);
      TEST_CHECK(parsed_until != NULL);
      TEST_CHECK(parsed_until[0] == 'x');
      FREE(&fmt);
    }
  }

  {
    static const struct TestCase no_fmt_tests[] = {
      // clang-format off
      { "0x",  '0', 0, -1, false, JUSTIFY_RIGHT },
      { "-0x", '0', 0, -1, false, JUSTIFY_LEFT  },
      { "0x",  '0', 0, -1, false, JUSTIFY_RIGHT },
      // clang-format off
    };

    struct ExpandoParseError err = { 0 };

    for (size_t i = 0; i < mutt_array_size(no_fmt_tests); i++)
    {
      const char *str = no_fmt_tests[i].src;
      const char *parsed_until = NULL;

      TEST_CASE(str);

      struct ExpandoFormat *fmt = parse_format(str, &parsed_until, &err);
      TEST_CHECK(fmt == NULL);
      TEST_CHECK(parsed_until != NULL);
      TEST_CHECK(parsed_until[0] == 'x');
    }
  }
}
