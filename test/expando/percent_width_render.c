/**
 * @file
 * Test code for percentage widths
 *
 * @authors
 * Copyright (C) 2026 David Logie <djl@mksh.org>
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
#include <stdio.h>
#include "mutt/lib.h"
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

static void pct_s(const struct ExpandoNode *node, void *data,
                  MuttFormatFlags flags, struct Buffer *buf)
{
  buf_strcpy(buf, "abcdefghijklmnopqrstuvwxyz");
}

struct TestCase
{
  const char *input;
  int cols;
  const char *expected;
};

void test_expando_percent_width_render(void)
{
  static const struct TestCase tests[] = {
    // clang-format off
    { "%-20%.20%s|",  50, "abcdefghij|"    },
    { "%-20%.20%s|", 100, "abcdefghijklmnopqrst|" },
    { "%.10%s|",      50, "abcde|"         },
    { "%=50%s|",      60, "  abcdefghijklmnopqrstuvwxyz  |" },
    { "%50%.4s|",     10, " abcd|"         },
    { "%-20%.20%s|",  -1, "abcdefghijklmnopqrstuvwxyz|" },
    { "%.5%s|",       10, "a|"             },
    { "%-50%<s?ab&cd>|", 10, "ab   |"      },
    { "%.5%<s?ab&cd>|",  10, "a|"          },
    // clang-format on
  };

  const struct ExpandoDefinition defs[] = {
    { "s", NULL, 1, 0, NULL },
    { NULL, NULL, 0, 0, NULL },
  };

  const struct ExpandoRenderCallback render[] = {
    { 1, 0, pct_s },
    { -1, -1, NULL },
  };

  struct Buffer *err = buf_pool_get();
  struct Buffer *buf = buf_pool_get();

  for (size_t i = 0; i < countof(tests); i++)
  {
    TEST_CASE_("'%s' at %d cols", tests[i].input, tests[i].cols);
    struct Expando *exp = expando_parse(tests[i].input, defs, err);
    TEST_CHECK(exp != NULL);
    TEST_CHECK(buf_is_empty(err));

    buf_reset(buf);
    expando_render(exp, render, NULL, MUTT_FORMAT_NONE, tests[i].cols, buf);
    TEST_CHECK_STR_EQ(buf_string(buf), tests[i].expected);
    expando_free(&exp);
  }

  // A percentage over 100 is an error
  struct Expando *exp = expando_parse("%101%s", defs, err);
  TEST_CHECK(exp == NULL);
  TEST_CHECK(!buf_is_empty(err));
  expando_free(&exp);

  buf_pool_release(&err);
  buf_pool_release(&buf);
}
