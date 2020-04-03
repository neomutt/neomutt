/**
 * @file
 * Test code for mutt_str_lws_len()
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

void test_mutt_str_lws_len(void)
{
  // size_t mutt_str_lws_len(const char *s, size_t n);

  // clang-format off
  struct LwsLenTest lws_tests[] =
  {
    { NULL,           10, 0 },
    { "",              1, 0 },
    { "apple",         6, 0 },
    { "apple",         0, 0 },

    { " apple",        7, 1 },
    { "\tapple",       7, 1 },
    { "\napple",       7, 0 },
    { "\rapple",       7, 0 },

    { " \t\n\rapple", 10, 0 },
    { " \t\n\r",       5, 0 },

    { "    apple",     5, 4 },
    { "     apple",    5, 5 },
    { "      apple",   5, 5 },
  };
  // clang-format on

  {
    for (size_t i = 0; i < mutt_array_size(lws_tests); i++)
    {
      struct LwsLenTest *t = &lws_tests[i];
      TEST_CASE_("%ld, '%s'", i, NONULL(t->str));
      size_t result = mutt_str_lws_len(t->str, t->len);
      TEST_CHECK(result == t->result);
    }
  }
}
