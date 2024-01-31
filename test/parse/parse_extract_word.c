/**
 * @file
 * Test code for extracting words from strings
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
#include "parse/lib.h"
#include "test_common.h"

struct ExtractTest
{
  const char *str;
  size_t pos;
  bool result;
  const char *word;
};

void test_parse_extract_word(void)
{
  // bool parse_extract_word(struct Buffer *dest, struct Buffer *buf)

  // clang-format off
  static const struct ExtractTest arg_tests[] =
  {
    { "hello # world",  0, true,  "hello"        },
    { "hello # world",  5, false, ""             },
    { "hello # world",  6, false, ""             },
    { "hello #world",   0, true,  "hello"        },
    { "hello#world",    0, true,  "hello"        },
    { "\"foo bar\"",    0, true,  "foo bar"      },
    { "foo\\ bar",      0, true,  "foo bar"      },
    { "foo \\\" # bar", 0, true,  "foo \" # bar" },
  };
  // clang-format on

  {
    for (size_t i = 0; i < mutt_array_size(arg_tests); i++)
    {
      const struct ExtractTest *t = &arg_tests[i];
      struct Buffer *buf = NULL;
      if (t->str)
      {
        buf = buf_new(t->str);
        buf->dptr = buf->data + t->pos;
      }

      TEST_CASE_("parse_extract_word(\"%s\"[%d]) == %d -> %s", buf_string(buf),
                 t->pos, t->result, t->word);
      struct Buffer *dest = buf_pool_get();
      TEST_CHECK(parse_extract_word(dest, buf) == t->result);
      TEST_CHECK_STR_EQ(buf_string(dest), t->word);
      buf_free(&buf);
    }
  }
}
