/**
 * @file
 * Test code for SKIPWS() macro
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
#include <string.h>
#include "mutt/lib.h"
#include "test_common.h"

/**
 * Test data structure for SKIPWS tests
 */
struct SkipwsTest
{
  const char *str;        ///< Input string
  size_t expected_offset; ///< Expected offset after skipping whitespace
};

void test_skipws(void)
{
  // SKIPWS(char *ch) - Skip whitespace characters
  // Macro modifies the pointer to skip past whitespace

  TEST_CASE("SKIPWS - Basic tests");

  // clang-format off
  struct SkipwsTest tests[] = {
    // Empty string - no movement
    { "",              0 },

    // No leading whitespace
    { "text",          0 },
    { "token arg",     0 },
    { "#comment",      0 },
    { ";semicolon",    0 },

    // Single whitespace characters
    { " text",         1 },
    { "\ttext",        1 },
    { "\ntext",        1 },
    { "\rtext",        1 },

    // Multiple whitespace characters
    { "  text",        2 },
    { "   text",       3 },
    { "\t\ttext",      2 },
    { "\n\ntext",      2 },

    // Mixed whitespace
    { " \ttext",       2 },
    { "\t text",       2 },
    { " \t\n\rtext",   4 },
    { "  \t  text",    5 },

    // All whitespace (should advance to end)
    { " ",             1 },
    { "  ",            2 },
    { "\t",            1 },
    { " \t \n",        4 },

    // Vertical tab and form feed are also whitespace per isspace()
    { "\vtext",        1 },
    { "\ftext",        1 },

    // Whitespace after text (not skipped - starts at text)
    { "text ",         0 },
    { "a b",           0 },
  };
  // clang-format on

  for (size_t i = 0; i < countof(tests); i++)
  {
    struct SkipwsTest *t = &tests[i];
    TEST_CASE_("SKIPWS: '%s'", t->str);

    // SKIPWS works on char pointers directly
    char buf[256];
    mutt_str_copy(buf, t->str, sizeof(buf));
    char *ptr = buf;

    SKIPWS(ptr);

    size_t actual_offset = ptr - buf;
    TEST_CHECK(actual_offset == t->expected_offset);
    TEST_MSG("Expected offset: %zu, Got: %zu", t->expected_offset, actual_offset);

    // Verify the pointer points to expected character
    if (t->expected_offset < strlen(t->str))
    {
      TEST_CHECK(*ptr == t->str[t->expected_offset]);
    }
    else
    {
      TEST_CHECK(*ptr == '\0');
    }
  }
}
