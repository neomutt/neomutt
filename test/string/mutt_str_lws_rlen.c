/**
 * @file
 * Test code for mutt_str_lws_rlen()
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

struct LwsLenTest
{
  const char *str;
  size_t len;
  size_t result;
};

void test_mutt_str_lws_rlen(void)
{
  // size_t mutt_str_lws_rlen(const char *s, size_t n);

  // clang-format off
  struct LwsLenTest lws_tests[] =
  {
    { NULL,           10, 0 },
    { "",              1, 0 },
    { "apple",         5, 0 },
    { "apple",         0, 0 },

    { "apple ",        6, 1 },
    { "apple\t",       6, 1 },
    { "apple\n",       6, 0 },
    { "apple\r",       6, 0 },

    { "apple \t\n\r", 10, 0 },
    { " \t\n\r",       5, 0 },

    { "apple    ",     8, 3 },
    { "apple    ",     7, 2 },
  };
  // clang-format on

  {
    for (size_t i = 0; i < mutt_array_size(lws_tests); i++)
    {
      struct LwsLenTest *t = &lws_tests[i];
      TEST_CASE_("%ld, '%s'", i, NONULL(t->str));
      size_t result = mutt_str_lws_rlen(t->str, t->len);
      TEST_CHECK(result == t->result);
    }
  }
}
