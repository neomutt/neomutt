/**
 * @file
 * Test code for Colour Parsing
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
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "color/lib.h"

int parse_color_prefix(const char *s, enum ColorPrefix *prefix);

struct PrefixTest
{
  const char *str;
  int len;
  enum ColorPrefix prefix;
};

void test_parse_color_prefix(void)
{
  // int parse_color_prefix(const char *s, enum ColorPrefix *prefix);

  {
    enum ColorPrefix prefix = COLOR_PREFIX_NONE;
    const char *test = "brightred";
    int len;

    len = parse_color_prefix(test, NULL);
    TEST_CHECK(len == 0);

    len = parse_color_prefix(NULL, &prefix);
    TEST_CHECK(len == 0);
  }

  {
    struct PrefixTest tests[] = {
      // clang-format off
      { "red",       0, COLOR_PREFIX_NONE   },
      { "brightred", 6, COLOR_PREFIX_BRIGHT },
      { "alertred",  5, COLOR_PREFIX_ALERT  },
      { "lightred",  5, COLOR_PREFIX_LIGHT  },
      { "bright",    6, COLOR_PREFIX_BRIGHT },
      { "alert",     5, COLOR_PREFIX_ALERT  },
      { "light",     5, COLOR_PREFIX_LIGHT  },
      { "BriGHtred", 6, COLOR_PREFIX_BRIGHT },
      { "AleRTred",  5, COLOR_PREFIX_ALERT  },
      { "LigHTred",  5, COLOR_PREFIX_LIGHT  },
      { "BriGHt",    6, COLOR_PREFIX_BRIGHT },
      { "AleRT",     5, COLOR_PREFIX_ALERT  },
      { "LigHT",     5, COLOR_PREFIX_LIGHT  },
      { "brigh",     0, COLOR_PREFIX_NONE   },
      { "aler",      0, COLOR_PREFIX_NONE   },
      { "ligh",      0, COLOR_PREFIX_NONE   },
      { NULL, 0 },
      // clang-format on
    };

    for (int i = 0; tests[i].str; i++)
    {
      TEST_CASE(tests[i].str);
      enum ColorPrefix prefix = COLOR_PREFIX_NONE;
      int len = parse_color_prefix(tests[i].str, &prefix);

      TEST_CHECK(len == tests[i].len);
      TEST_MSG("\tlen:    Expected %d, Got %d\n", tests[i].len, len);

      TEST_CHECK(prefix == tests[i].prefix);
      TEST_MSG("\tprefix: Expected %d, Got %d\n", tests[i].prefix, prefix);
    }
  }
}
