/**
 * @file
 * Test code for mutt_str_strnlower()
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

struct TestString
{
  char *src;
  char *result;
  int len;
};

void test_mutt_str_strnlower(void)
{
  // char *mutt_str_strnlower(char *str, size_t num);

  static const struct TestString tests[] = {
    { "", "", 0 },       { "", "", 10 },      { "abc", "abc", 3 },
    { "AbC", "abc", 3 }, { "ABC", "abC", 2 }, { "$%^", "$%^", 3 },
  };

  {
    TEST_CHECK(mutt_str_strnlower(NULL, 0) == NULL);
  }

  {
    char buf[32];
    char *result = NULL;

    for (size_t i = 0; i < mutt_array_size(tests); i++)
    {
      TEST_CASE_("'%s', %d", tests[i].src, tests[i].len);
      mutt_str_strfcpy(buf, tests[i].src, sizeof(buf));
      result = mutt_str_strnlower(buf, tests[i].len);
      if (!TEST_CHECK(mutt_str_equal(result, tests[i].result, CASE_MATCH)))
      {
        TEST_MSG("Expected: '%s'", tests[i].result);
        TEST_MSG("Actual:   '%s'", buf);
      }
    }
  }
}
