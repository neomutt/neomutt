/**
 * @file
 * Test code for checking for tokens in strings
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
  size_t pos;
  bool result;
};

void test_has_more_tokens(void)
{
  // bool has_more_tokens(struct Buffer *buf);

  // clang-format off
  static const struct ArgTest arg_tests[] =
  {
    { NULL, 0, false },
    { "",   0, false },

    { "apple",  0, true  },
    { "apple;", 4, true  },
    { "apple;", 5, false },

    { "apple#", 0, true  },
    { "apple#", 4, true  },
    { "apple#", 5, false },

    { "apple # orange", 0, true  },
    { "apple # orange", 5, true  },
    { "apple # orange", 6, false },

    { "apple; orange", 0, true  },
    { "apple; orange", 4, true  },
    { "apple; orange", 5, false },

    { "foo#bar", 3, false },
    { "foo+bar", 3, true  },
    { "foo-bar", 3, true  },
    { "foo=bar", 3, true  },
    { "foo?bar", 3, true  },
    { "foo;bar", 3, false },
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

      TEST_CASE_("has_more_tokens(\"%s\"[%d]) == %d", buf_string(buf), t->pos, t->result);
      TEST_CHECK(has_more_tokens(buf) == t->result);
      buf_free(&buf);
    }
  }
}
