/**
 * @file
 * Test Expando Text Parse
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
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

struct TextTest
{
  const char *input;
  NodeTextTermFlags flags;
  const char *expected;
  char terminator;
};

void test_expando_node_text_parse(void)
{
  // struct ExpandoNode *node_text_parse(const char *str, NodeTextTermFlags term_chars, const char **parsed_until);

  // Degenerate tests
  {
    TEST_CASE("degenerate");
    struct ExpandoNode *node = NULL;
    const char *parsed_until = NULL;

    node = node_text_parse(NULL, NTE_NO_FLAGS, &parsed_until);
    TEST_CHECK(node == NULL);

    node = node_text_parse("", NTE_NO_FLAGS, &parsed_until);
    TEST_CHECK(node == NULL);

    node = node_text_parse("apple", NTE_NO_FLAGS, NULL);
    TEST_CHECK(node == NULL);
  }

  static const struct TextTest tests[] = {
    // clang-format off
    { "apple",     NTE_NO_FLAGS, "apple",   '\0' },
    { "ap\\ple",   NTE_NO_FLAGS, "apple",   '\0' },
    { "ap\\\\ple", NTE_NO_FLAGS, "ap\\ple", '\0' },
    { "apple\\",   NTE_NO_FLAGS, "apple\\", '\0' },
    { "app%le",    NTE_NO_FLAGS, "app",     '%'  },
    { "app%%le",   NTE_NO_FLAGS, "app%le",  '\0' },
    { "app\\%le",  NTE_NO_FLAGS, "app%le",  '\0' },
    { "app\\&le",  NTE_NO_FLAGS, "app&le",  '\0' },
    { "app\\>le",  NTE_NO_FLAGS, "app>le",  '\0' },
    { "app\\?le",  NTE_NO_FLAGS, "app?le",  '\0' },

    { "banana",    NTE_AMPERSAND, "banana",  '\0' },
    { "ban&ana",   NTE_AMPERSAND, "ban",     '&'  },
    { "ba>n&a?na", NTE_AMPERSAND, "ba>n",    '&'  },
    { "ban\\&ana", NTE_AMPERSAND, "ban&ana", '\0' },
    { "banana",    NTE_AMPERSAND, "banana",  '\0' },
    { "banana",    NTE_AMPERSAND, "banana",  '\0' },

    { "cherry",    NTE_GREATER, "cherry",  '\0' },
    { "che>rry",   NTE_GREATER, "che",     '>'  },
    { "ch&e>r?ry", NTE_GREATER, "ch&e",    '>'  },
    { "che\\>rry", NTE_GREATER, "che>rry", '\0' },
    { "cherry",    NTE_GREATER, "cherry",  '\0' },
    { "cherry",    NTE_GREATER, "cherry",  '\0' },

    { "damson",    NTE_QUESTION, "damson",  '\0' },
    { "dam?son",   NTE_QUESTION, "dam",     '?'  },
    { "da&m?s>on", NTE_QUESTION, "da&m",    '?'  },
    { "dam\\?son", NTE_QUESTION, "dam?son", '\0' },
    { "damson",    NTE_QUESTION, "damson",  '\0' },
    { "damson",    NTE_QUESTION, "damson",  '\0' },

    { "endive",          NTE_AMPERSAND | NTE_GREATER | NTE_QUESTION, "endive",    '\0' },
    { "end&ive",         NTE_AMPERSAND | NTE_GREATER | NTE_QUESTION, "end",       '&'  },
    { "end>ive",         NTE_AMPERSAND | NTE_GREATER | NTE_QUESTION, "end",       '>'  },
    { "end?ive",         NTE_AMPERSAND | NTE_GREATER | NTE_QUESTION, "end",       '?'  },
    { "en\\&d\\?i\\>ve", NTE_AMPERSAND | NTE_GREATER | NTE_QUESTION, "en&d?i>ve", '\0' },

    { NULL, NTE_NO_FLAGS, NULL },
    // clang-format on
  };

  {
    for (int i = 0; tests[i].input; i++)
    {
      TEST_CASE(tests[i].input);

      const char *parsed_until = NULL;
      struct ExpandoNode *node = node_text_parse(tests[i].input, tests[i].flags, &parsed_until);

      TEST_CHECK(node != NULL);
      TEST_CHECK_STR_EQ(node->text, tests[i].expected);
      TEST_CHECK(parsed_until != NULL);
      TEST_CHECK(parsed_until[0] == tests[i].terminator);

      node_free(&node);
    }
  }
}
