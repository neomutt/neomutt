/**
 * @file
 * Test code for MoreArgs() and MoreArgsF() macros
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
#include "parse/lib.h"
#include "test_common.h"

/**
 * Test data structure for MoreArgs tests
 */
struct MoreArgsTest
{
  const char *str;        ///< Input string
  bool expected;          ///< Expected result from MoreArgs()
};

/**
 * Test data structure for MoreArgsF tests
 */
struct MoreArgsFTest
{
  const char *str;        ///< Input string
  TokenFlags flags;       ///< Flags to pass to MoreArgsF()
  bool expected;          ///< Expected result from MoreArgsF()
};

void test_more_args(void)
{
  // bool more_args(struct Buffer *buf, TokenFlags flags)
  // Returns true if there are more arguments to parse
  // MoreArgs(buf) is a macro that calls more_args(buf, TOKEN_SPACE)

  TEST_CASE("more_args - Basic tests (legacy MoreArgs behavior)");

  // clang-format off
  struct MoreArgsTest tests[] = {
    // Empty string - no more args
    { "",           false },

    // Whitespace only - still checks first char
    { " ",          true  },
    { "  ",         true  },
    { "\t",         true  },

    // Simple tokens
    { "token",      true  },
    { "token arg",  true  },

    // Comments - no more args
    { "#comment",   false },
    { "# comment",  false },

    // Semicolons - end of line - no more args
    { ";",          false },
    { "; next",     false },

    // Mixed cases
    { "arg1 arg2",  true  },
    { "arg1 #",     true  },
    { "arg1;arg2",  true  },
  };
  // clang-format on

  for (size_t i = 0; i < countof(tests); i++)
  {
    struct MoreArgsTest *t = &tests[i];
    TEST_CASE_("MoreArgs: '%s'", t->str);

    struct Buffer *buf = buf_pool_get();
    buf_strcpy(buf, t->str);
    buf_seek(buf, 0);

    bool result = MoreArgs(buf);
    TEST_CHECK(result == t->expected);
    TEST_MSG("Expected: %s, Got: %s", t->expected ? "true" : "false", result ? "true" : "false");

    buf_pool_release(&buf);
  }
}

void test_more_args_f(void)
{
  // bool more_args(struct Buffer *buf, TokenFlags flags)
  // Returns true if there are more arguments to parse based on flags

  TEST_CASE("more_args - Basic tests with flags");

  // clang-format off
  struct MoreArgsFTest tests[] = {
    // Empty string
    { "",           TOKEN_NO_FLAGS, false },

    // Whitespace - with TOKEN_SPACE flag, don't treat as terminator
    { " ",          TOKEN_NO_FLAGS, false },
    { " ",          TOKEN_SPACE,    true  },
    { "\t",         TOKEN_NO_FLAGS, false },
    { "\t",         TOKEN_SPACE,    true  },

    // Comments - with TOKEN_COMMENT flag, don't treat # as terminator
    { "#comment",   TOKEN_NO_FLAGS, false },
    { "#comment",   TOKEN_COMMENT,  true  },
    { "# comment",  TOKEN_NO_FLAGS, false },
    { "# comment",  TOKEN_COMMENT,  true  },

    // Semicolon - with TOKEN_SEMICOLON flag, don't treat ; as terminator
    { ";",          TOKEN_NO_FLAGS, false },
    { ";",          TOKEN_SEMICOLON, true  },
    { "; next",     TOKEN_NO_FLAGS, false },
    { "; next",     TOKEN_SEMICOLON, true  },

    // Plus sign - with TOKEN_PLUS flag, treat + as terminator
    { "+",          TOKEN_NO_FLAGS, true  },
    { "+",          TOKEN_PLUS,     false },
    { "+value",     TOKEN_NO_FLAGS, true  },
    { "+value",     TOKEN_PLUS,     false },

    // Minus sign - with TOKEN_MINUS flag, treat - as terminator
    { "-",          TOKEN_NO_FLAGS, true  },
    { "-",          TOKEN_MINUS,    false },
    { "-value",     TOKEN_NO_FLAGS, true  },
    { "-value",     TOKEN_MINUS,    false },

    // Equal sign - with TOKEN_EQUAL flag, treat = as terminator
    { "=",          TOKEN_NO_FLAGS, true  },
    { "=",          TOKEN_EQUAL,    false },
    { "=value",     TOKEN_NO_FLAGS, true  },
    { "=value",     TOKEN_EQUAL,    false },

    // Question mark - with TOKEN_QUESTION flag, treat ? as terminator
    { "?",          TOKEN_NO_FLAGS, true  },
    { "?",          TOKEN_QUESTION, false },
    { "?value",     TOKEN_NO_FLAGS, true  },
    { "?value",     TOKEN_QUESTION, false },

    // Pattern characters - with TOKEN_PATTERN flag, ~%=!| are pattern chars (continue parsing)
    // Without TOKEN_PATTERN, regular text is allowed (not a terminator)
    // With TOKEN_PATTERN, these characters ARE expected (continue parsing)
    { "~pattern",   TOKEN_NO_FLAGS, true  },
    { "~pattern",   TOKEN_PATTERN,  true  },  // ~ is a pattern char, so more args
    { "%pattern",   TOKEN_NO_FLAGS, true  },
    { "%pattern",   TOKEN_PATTERN,  true  },  // % is a pattern char, so more args
    { "=pattern",   TOKEN_NO_FLAGS, true  },
    { "=pattern",   TOKEN_PATTERN,  true  },  // = is a pattern char, so more args
    { "!pattern",   TOKEN_NO_FLAGS, true  },
    { "!pattern",   TOKEN_PATTERN,  true  },  // ! is a pattern char, so more args
    { "|pattern",   TOKEN_NO_FLAGS, true  },
    { "|pattern",   TOKEN_PATTERN,  true  },  // | is a pattern char, so more args
    // Regular text without pattern chars
    { "text",       TOKEN_PATTERN,  false },  // 't' is not a pattern char, so no more args

    // Regular text - always has more args
    { "text",       TOKEN_NO_FLAGS, true  },
    { "text",       TOKEN_SPACE,    true  },
    { "text",       TOKEN_COMMENT,  true  },

    // Combined flags
    { " #text",     TOKEN_SPACE | TOKEN_COMMENT, true },
    { ";#text",     TOKEN_SEMICOLON | TOKEN_COMMENT, true },
  };
  // clang-format on

  for (size_t i = 0; i < countof(tests); i++)
  {
    struct MoreArgsFTest *t = &tests[i];
    TEST_CASE_("MoreArgsF: '%s' flags=%u", t->str, t->flags);

    struct Buffer *buf = buf_pool_get();
    buf_strcpy(buf, t->str);
    buf_seek(buf, 0);

    bool result = MoreArgsF(buf, t->flags);
    TEST_CHECK(result == t->expected);
    TEST_MSG("Expected: %s, Got: %s", t->expected ? "true" : "false", result ? "true" : "false");

    buf_pool_release(&buf);
  }
}
