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
#include "gui/lib.h"
#include "color/lib.h"

struct AnsiTest
{
  const char *str;
  int len;
  int attrs;
};

void test_ansi_color_parse_single(void)
{
  // int ansi_color_parse_single(const char *buf, struct AnsiColor *ansi, bool dry_run);

  // Degenerate tests
  {
    struct AnsiColor ansi = { 0 };
    const char *str = "\033[31m";
    int len;

    len = ansi_color_parse_single(NULL, &ansi, false);
    TEST_CHECK(len == 0);

    len = ansi_color_parse_single(str, NULL, false);
    TEST_CHECK(len == 5);

    len = ansi_color_parse_single("", &ansi, false);
    TEST_CHECK(len == 0);

    len = ansi_color_parse_single(str, &ansi, true);
    TEST_CHECK(len == 5);
  }

  // Attributes
  {
    static const struct AnsiTest tests[] = {
      // clang-format off
      { "\033[m",  3, A_NORMAL    },
      { "\033[0m", 4, A_NORMAL    },
      { "\033[1m", 4, A_BOLD      },
      { "\033[3m", 4, A_ITALIC    },
      { "\033[4m", 4, A_UNDERLINE },
      { "\033[5m", 4, A_BLINK     },
      { "\033[7m", 4, A_REVERSE   },
      { NULL, 0 },
      // clang-format on
    };

    int len;
    for (int i = 0; tests[i].str; i++)
    {
      TEST_CASE_("<esc>%s", tests[i].str + 1);

      struct AnsiColor ansi = { 0 };
      len = ansi_color_parse_single(tests[i].str, &ansi, false);
      TEST_CHECK(len == tests[i].len);
      TEST_MSG("len: Expected %d, Got %d", tests[i].len, len);
      TEST_CHECK(ansi.attrs == tests[i].attrs);
      TEST_MSG("attrs: Expected %d, Got %d", tests[i].attrs, ansi.attrs);
    }
  }

  // Simple Colours - Foregrounds and Backgrounds
  {
    char str[32];
    int len;
    for (int i = 30; i < 41; i += 10)
    {
      for (int j = 0; j < 8; j++)
      {
        snprintf(str, sizeof(str), "\033[%dm", i + j);
        TEST_CASE_("<esc>%s", str + 1);

        struct AnsiColor ansi = { 0 };
        len = ansi_color_parse_single(str, &ansi, false);
        TEST_CHECK(len == 5);
        TEST_MSG("len: Expected 5, Got %d", len);
      }
    }
  }

  // Palette Colours - Foregrounds and Backgrounds
  {
    char str[32];
    for (int i = 38; i < 49; i += 10)
    {
      for (int j = 0; j < 255; j++)
      {
        snprintf(str, sizeof(str), "\033[%d;5;%dm", i, j);
        TEST_CASE_("<esc>%s", str + 1);

        struct AnsiColor ansi = { 0 };
        int len = ansi_color_parse_single(str, &ansi, false);
        int expected = 9 + ((j > 9) ? 1 : 0) + ((j > 99) ? 1 : 0);
        TEST_CHECK(len == expected);
        TEST_MSG("len: Expected %d, Got %d", expected, len);
      }
    }
  }

  // RGB Colours - Foregrounds and Backgrounds
  {
    static const int red[] = { 0,   1,   67,  189, 31,  103, 121, 162,
                               142, 174, 100, 87,  254, 255, -1 };
    static const int green[] = { 0,  1,  86,  214, 142, 29,  87, 89,
                                 75, 28, 170, 97,  254, 255, -1 };
    static const int blue[] = { 0,   1,   200, 142, 239, 107, 125, 179,
                                198, 190, 189, 246, 254, 255, -1 };

    char str[32];
    for (int i = 38; i < 49; i += 10)
    {
      for (int r = 0; red[r] >= 0; r++)
      {
        for (int g = 0; green[g] >= 0; g++)
        {
          for (int b = 0; blue[b] >= 0; b++)
          {
            int val_r = red[r];
            int val_g = green[g];
            int val_b = blue[b];
            snprintf(str, sizeof(str), "\033[%d;2;%d;%d;%dm", i, val_r, val_g, val_b);
            TEST_CASE_("<esc>%s", str + 1);

            struct AnsiColor ansi = { 0 };
            int len = ansi_color_parse_single(str, &ansi, false);
            int expected = 13 + ((val_r > 9) ? 1 : 0) + ((val_r > 99) ? 1 : 0) +
                           ((val_g > 9) ? 1 : 0) + ((val_g > 99) ? 1 : 0) +
                           ((val_b > 9) ? 1 : 0) + ((val_b > 99) ? 1 : 0);
            TEST_CHECK(len == expected);
            TEST_MSG("len: Expected %d, Got %d", expected, len);
          }
        }
      }
    }
  }
}
