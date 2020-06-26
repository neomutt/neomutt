/**
 * @file
 * Test code for mutt_str_remove_trailing_ws()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
#include "mutt/lib.h"

struct TrailTest
{
  const char *str;
  const char *expected;
};

void test_mutt_str_remove_trailing_ws(void)
{
  // void mutt_str_remove_trailing_ws(char *s);

  {
    mutt_str_remove_trailing_ws(NULL);
    TEST_CHECK_(1, "mutt_str_remove_trailing_ws(NULL)");
  }

  // none
  // trailing
  // only whitespace

  // clang-format off
  struct TrailTest trail_tests[] =
  {
    { "",              ""         },

    { "hello ",        "hello"    },
    { "hello\t",       "hello"    },
    { "hello\r",       "hello"    },
    { "hello\n",       "hello"    },

    { "hello \t",      "hello"    },
    { "hello\t ",      "hello"    },
    { "hello\r\t",     "hello"    },
    { "hello\n\r",     "hello"    },

    { " \n  \r \t\t ", ""         },
  };
  // clang-format on

  {
    char buf[64];

    for (size_t i = 0; i < mutt_array_size(trail_tests); i++)
    {
      struct TrailTest *t = &trail_tests[i];
      memset(buf, 0, sizeof(buf));
      mutt_str_copy(buf, t->str, sizeof(buf));
      TEST_CASE_("'%s'", buf);

      mutt_str_remove_trailing_ws(buf);
      TEST_CHECK(strcmp(buf, t->expected) == 0);
    }
  }
}
