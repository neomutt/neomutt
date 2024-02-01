/**
 * @file
 * Test code for buf_rfind()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"

struct RfindTest
{
  const char *str;
  bool success;
  size_t offset;
};

void test_buf_rfind(void)
{
  // const char *buf_rfind(const struct Buffer *buf, const char *str);

  // clang-format off
  struct RfindTest rstrn_tests[] =
  {
    { NULL,             false, 0 },
    { "",               false, 0 },
    { "text",           false, 0 },

    { "appleTEXT",      true, 0  },
    { "TEXTappleTEXT",  true, 4  },
    { "TEXTapple",      true, 4  },

    { "TEXTappleapple", true, 9  },
    { "appleTEXTapple", true, 9  },
    { "appleappleTEXT", true, 5  },
  };
  // clang-format on

  {
    const char *find = "apple";

    for (size_t i = 0; i < mutt_array_size(rstrn_tests); i++)
    {
      struct Buffer *buf = NULL;

      struct RfindTest *t = &rstrn_tests[i];
      if (t->str)
        buf = buf_new(t->str);

      TEST_CASE_("buf_rfind('%s', '%s') == %d", t->str, find, t->offset);

      const char *result = buf_rfind(buf, find);
      if (t->success)
        TEST_CHECK(result == buf->data + t->offset);
      else
        TEST_CHECK(result == NULL);

      buf_free(&buf);
    }
  }
}
