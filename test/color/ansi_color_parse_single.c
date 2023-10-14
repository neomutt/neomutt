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

void ansi_color_reset(struct AnsiColor *ansi);
int ansi_skip_sequence(const char *str);

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

    ansi_color_reset(NULL);

    ansi_skip_sequence(NULL);
    ansi_skip_sequence("");
  }

  // Skip
  {
    static const struct Mapping tests[] = {
      // clang-format off
      { "\033[m",         3 },
      { "\033[1m",        4 },
      { "\033[3m",        4 },
      { "\033[03m",       5 },
      { "\033[48;5;123m", 5 },
      { "\033[5;22m",     4 },
      { NULL, 0 },
      // clang-format on
    };

    int len;
    for (int i = 0; tests[i].name; i++)
    {
      TEST_CASE_("<esc>%s", tests[i].name + 1);

      len = ansi_skip_sequence(tests[i].name);
      TEST_CHECK(len == tests[i].value);
      TEST_MSG("len: Expected %d, Got %d", tests[i].value, len);
    }
  }

  // Length
  {
    static const struct Mapping tests[] = {
      // clang-format off
      { NULL,              0 },
      { "",                0 },
      { "apple",           0 },
      { "\033]apple",      0 },
      { "\033[3m",         4 },
      { "\033[48;5;123m", 11 },
      { "\033[5;22m",      7 },
      { "\033[5;22c",      0 },
      // clang-format on
    };

    int len;
    for (int i = 0; i < mutt_array_size(tests); i++)
    {
      TEST_CASE_("Length %d", i);

      len = ansi_color_seq_length(tests[i].name);
      TEST_CHECK(len == tests[i].value);
      TEST_MSG("len: Expected %d, Got %d", tests[i].value, len);
    }
  }

  // Attributes
  {
    static const struct AnsiTest tests[] = {
      // clang-format off
      { "\033[m",   3, A_NORMAL    },
      { "\033[0m",  4, A_NORMAL    },
      { "\033[1m",  4, A_BOLD      },
      { "\033[3m",  4, A_ITALIC    },
      { "\033[03m", 5, A_ITALIC    },
      { "\033[4m",  4, A_UNDERLINE },
      { "\033[5m",  4, A_BLINK     },
      { "\033[7m",  4, A_REVERSE   },
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

  // Cancel Attributes
  {
    static const struct Mapping tests[] = {
      // clang-format off
      { "\033[1;22m", 7 },
      { "\033[3;23m", 7 },
      { "\033[4;24m", 7 },
      { "\033[5;25m", 7 },
      { "\033[7;27m", 7 },
      { "\033[39m", 5 },
      { "\033[49m", 5 },
      // clang-format on
    };

    int len;
    for (int i = 0; i < mutt_array_size(tests); i++)
    {
      TEST_CASE_("Cancel %d", i);

      struct AnsiColor ansi = { 0 };
      len = ansi_color_parse_single(tests[i].name, &ansi, false);
      TEST_CHECK(len == tests[i].value);
      TEST_MSG("len: Expected %d, Got %d", tests[i].value, len);
      TEST_CHECK(ansi.attrs == A_NORMAL);
      TEST_MSG("attrs: Expected %d, Got %d", A_NORMAL, ansi.attrs);
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
    // clang-format off
    static const int red[]   = { 0, 1, 67,  189, 31,  103, 121, 162, 142, 174, 100, 87,  254, 255 };
    static const int green[] = { 0, 1, 86,  214, 142, 29,  87,  89,  75,  28,  170, 97,  254, 255 };
    static const int blue[]  = { 0, 1, 200, 142, 239, 107, 125, 179, 198, 190, 189, 246, 254, 255 };
    // clang-format on

    char str[32];
    for (int i = 38; i < 49; i += 10)
    {
      for (int r = 0; r < mutt_array_size(red); r++)
      {
        for (int g = 0; g < mutt_array_size(green); g++)
        {
          for (int b = 0; b < mutt_array_size(blue); b++)
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

  // Bad Colours
  {
    static const char *tests[] = {
      // clang-format off
      "\033[3X",
      "\033[30X",
      "\033[37X",
      "\033[39X",
      "\033[38X",
      "\033[38;X",
      "\033[38;5X",
      "\033[38;5;X",
      "\033[38;5;12X",
      "\033[38;5;500m",
      "\033[38;2X",
      "\033[38;2;X",
      "\033[38;2;12X",
      "\033[38;2;500;m",
      "\033[38;2;12;X",
      "\033[38;2;12;34X",
      "\033[38;2;12;500;m",
      "\033[38;2;12;34;X",
      "\033[38;2;12;34;56X",
      "\033[38;2;12;34;500m",
      "\033[4X",
      "\033[40X",
      "\033[47X",
      "\033[49X",
      "\033[48X",
      "\033[48;X",
      "\033[48;5X",
      "\033[48;5;X",
      "\033[48;5;12X",
      "\033[48;5;500m",
      "\033[48;2X",
      "\033[48;2;X",
      "\033[48;2;12X",
      "\033[48;2;500;m",
      "\033[48;2;12;X",
      "\033[48;2;12;34X",
      "\033[48;2;12;500;m",
      "\033[48;2;12;34;X",
      "\033[48;2;12;34;56X",
      "\033[48;2;12;34;500m",
      // clang-format on
    };

    int len;
    for (int i = 0; i < mutt_array_size(tests); i++)
    {
      TEST_CASE_("Bad %d", i);

      struct AnsiColor ansi = { 0 };
      len = ansi_color_parse_single(tests[i], &ansi, false);
      TEST_CHECK(len == 0);
      TEST_MSG("len: Expected 0, Got %d", len);
    }
  }
}
