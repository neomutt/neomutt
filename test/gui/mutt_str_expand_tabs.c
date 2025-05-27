/**
 * @file
 * Test code for mutt_str_expand_tabs()
 *
 * @authors
 * Copyright (C) 2025 Dennis Schön <mail@dennis-schoen.de>
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
#include "gui/lib.h"
#include "test_common.h"

struct TestCase
{
  char *in;
  char *result;
  int tabwidth;
};

void test_mutt_str_expand_tabs(void)
{
  // char *mutt_str_expand_tabs(char *str, size_t *len, int tabwidth);

  {
    TEST_CHECK(mutt_str_expand_tabs(NULL, 0, 4) == NULL);
    TEST_CHECK(mutt_str_expand_tabs("", 0, 4) == NULL);
  }

  static struct TestCase tests[] = {
    // clang-format off
    { "\tapple",      "    apple",     4 },
    { "X\tapple",     "X   apple",     4 },
    { "XX\tapple",    "XX  apple",     4 },
    { "XXX\tapple",   "XXX apple",     4 },
    { "XXXX\tapple",  "XXXX    apple", 4 },
    { "XXXXX\tapple", "XXXXX   apple", 4 },
    { "\tapple\t",    "    apple   ",  4 },
    { "🐛\tapple",    "🐛  apple",     4 },
    { "\t🐛\tapple",  "    🐛  apple", 4 },
    { "\t\tapple",    "        apple", 4 },
    { "X\t\tapple",   "X       apple", 4 },
    { "XX\t\tapple",  "XX      apple", 4 },
    { "XXX\t\tapple", "XXX     apple", 4 },
    { "\tapple",      "        apple", 8 },
    { "X\tapple",     "X       apple", 8 },
    { "XX\tapple",    "XX      apple", 8 },
    { "XXX\tapple",   "XXX     apple", 8 },
    // clang-format on
  };

  {
    for (size_t i = 0; i < countof(tests); i++)
    {
      TEST_CASE(tests[i].in);
      char *str = mutt_str_dup(tests[i].in);
      size_t len = mutt_str_len(str);
      char *result = mutt_str_expand_tabs(str, &len, tests[i].tabwidth);
      TEST_CHECK_STR_EQ(result, tests[i].result);
      FREE(&result);
    }
  }
}
