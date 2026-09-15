/**
 * @file
 * Test code for parse_keys() / keymap_get_name() with the modern key grammar
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
#include <string.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "key/lib.h"
#include "test_common.h"

/// Test entry for parse_keys()
struct ParseKeysTest
{
  const char *input;     ///< Input string passed to parse_keys()
  size_t expected_len;   ///< Expected number of keycodes
  keycode_t expected[8]; ///< Expected keycodes
};

/// Test entry for keymap_get_name()
struct GetNameTest
{
  int code;             ///< Input keycode
  const char *expected; ///< Expected rendered string
};

void test_parse_keys(void)
{
  // size_t parse_keys(const char *str, keycode_t *d, size_t max);

  static const struct ParseKeysTest tests[] = {
    // clang-format off
    // Plain printable
    { "a",                1, { 'a' } },
    { "Z",                1, { 'Z' } },
    { "gg",               2, { 'g', 'g' } },

    // Raw control byte (as the tokenizer would have produced from `\Ca`)
    { "\x01",             1, { 0x01 } },

    // Modern <C-x> grammar
    { "<C-a>",            1, { 0x01 } },
    { "<c-A>",            1, { 0x01 } },
    { "<C-z>",            1, { 0x1a } },
    { "<C-?>",            1, { 0x7f } },
    { "<C-[>",            1, { 0x1b } },

    // Modern <A-x> grammar -> ESC + base
    { "<A-x>",            2, { 0x1b, 'x' } },
    { "<A-?>",            2, { 0x1b, '?' } },

    // <M-x> alias for <A-x>
    { "<M-x>",            2, { 0x1b, 'x' } },

    // <S-x> on a letter just uppercases
    { "<S-a>",            1, { 'A' } },

    // Combined modifiers - order independent
    { "<C-A-x>",          2, { 0x1b, 0x18 } },
    { "<A-C-x>",          2, { 0x1b, 0x18 } },
    { "<M-C-x>",          2, { 0x1b, 0x18 } },
    { "<C-S-a>",          1, { 0x01 } },

    // Multi-key sequences via concatenation
    { "g<C-a>",           2, { 'g', 0x01 } },
    { "<A-x>g",           3, { 0x1b, 'x', 'g' } },
    { "<A-x><A-y>",       4, { 0x1b, 'x', 0x1b, 'y' } },
    { "<C-a><C-b><C-c>",  3, { 0x01, 0x02, 0x03 } },

    // Legacy octal `<NNN>` form (still works alongside the modern grammar)
    { "<177>",            1, { 0x7f } },
    { "<033>",            1, { 0x1b } },

    // Legacy F-key form
    { "<f12>",            1, { KEY_F(12) } },

    { NULL,               0, { 0 } },
    // clang-format on
  };

  keycode_t out[8];
  for (int i = 0; tests[i].input; i++)
  {
    TEST_CASE(tests[i].input);
    memset(out, 0, sizeof(out));
    size_t n = parse_keys(tests[i].input, out, countof(out));
    TEST_CHECK_NUM_EQ(n, tests[i].expected_len);
    for (size_t j = 0; j < tests[i].expected_len; j++)
    {
      TEST_CHECK_NUM_EQ(out[j], tests[i].expected[j]);
    }
  }
}

void test_keymap_get_name(void)
{
  // void keymap_get_name(int c, struct Buffer *buf);

  static const struct GetNameTest tests[] = {
    // clang-format off
    // Control bytes use modern <C-x> form
    { 0x01, "<C-a>"  },
    { 0x1a, "<C-z>"  },
    { 0x7f, "<C-?>"  },
    { 0x1c, "<C-\\>" },
    { 0x1d, "<C-]>"  },
    { 0x1e, "<C-^>"  },
    { 0x1f, "<C-_>"  },

    // Named keys still use their friendly names
    { 0x09, "<Tab>"    },
    { 0x0a, "<Enter>"  },
    { 0x0d, "<Return>" },
    { 0x1b, "<Esc>"    },
    { ' ',  "<Space>"  },

    // Plain printable
    { 'a', "a" },
    { 'A', "A" },
    { '5', "5" },

    { -1, NULL },
    // clang-format on
  };

  struct Buffer *buf = buf_pool_get();
  for (int i = 0; tests[i].expected; i++)
  {
    TEST_CASE(tests[i].expected);
    buf_reset(buf);
    keymap_get_name(tests[i].code, buf);
    TEST_CHECK_STR_EQ(buf_string(buf), tests[i].expected);
  }
  buf_pool_release(&buf);
}
