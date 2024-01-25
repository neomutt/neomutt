/**
 * @file
 * Test code for extracting tokens from strings
 *
 * @authors
 * Copyright (C) 2024 Dennis Schön <mail@dennis-schoen.de>
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

#include "parse/extract.h"
#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <string.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "parse/lib.h"

struct ArgTest
{
  const char *str;
  int flags;
  size_t pos;
  bool result;
};

void test_has_more_argsf(void)
{
  // bool has_more_argsf(struct Buffer *buf, int flags);

  // clang-format off
  static const struct ArgTest arg_tests[] =
  {
    { NULL, TOKEN_NO_FLAGS, 0, false },
    { "",   TOKEN_NO_FLAGS, 0, false },

    { "foo bar", TOKEN_NO_FLAGS, 3, false },
    { "foo#bar", TOKEN_NO_FLAGS, 3, false },
    { "foo+bar", TOKEN_NO_FLAGS, 3, true  },
    { "foo-bar", TOKEN_NO_FLAGS, 3, true  },
    { "foo=bar", TOKEN_NO_FLAGS, 3, true  },
    { "foo?bar", TOKEN_NO_FLAGS, 3, true  },
    { "foo;bar", TOKEN_NO_FLAGS, 3, false },

    // TODO: add more tests
  };
  // clang-format on

  {
    for (size_t i = 0; i < mutt_array_size(arg_tests); i++)
    {
      const struct ArgTest *t = &arg_tests[i];
      struct Buffer *buf = NULL;
      if (t->str)
      {
        buf = buf_new(t->str);
        buf->dptr = buf->data + t->pos;
      }

      TEST_CASE_("has_more_argsf(\"%s\"[%d], %d) == %d", buf_string(buf),
                 t->pos, t->flags, t->result);
      TEST_CHECK(has_more_argsf(buf, t->flags) == t->result);
      buf_free(&buf);
    }
  }
}
