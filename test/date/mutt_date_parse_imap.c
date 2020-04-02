/**
 * @file
 * Test code for mutt_date_parse_imap()
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

struct ParseImapTest
{
  const char *str;
  time_t expected;
};

void test_mutt_date_parse_imap(void)
{
  // time_t mutt_date_parse_imap(const char *s);

  {
    TEST_CHECK(mutt_date_parse_imap(NULL) == 0);
  }

  // clang-format off
  static struct ParseImapTest imap_tests[] = {
    // DD-MMM-YYYY HH:MM:SS +ZZzz
    { "12-Jan-1999 12:34:56 +0100", 916140896 },
    { " 2-Jan-1999 12:34:56 +0100", 915276896 },
    { "02-Jan-1999 12:34:56 +0100", 915276896 },
    { "12 Jan-1999 12:34:56 +0100", 0         },
    { "12-Jan 1999 12:34:56 +0100", 0         },
    { "12-Jan-1999-12:34:56 +0100", 0         },
    { "12-Jan-1999 12.34:56 +0100", 0         },
    { "12-Jan-1999 12:34.56 +0100", 0         },
    { "12-Jan-1999 12:34:56-+0100", 0         },
  };
  // clang-format on

  {
    for (size_t i = 0; i < mutt_array_size(imap_tests); i++)
    {
      TEST_CASE(imap_tests[i]);
      time_t result = mutt_date_parse_imap(imap_tests[i].str);
      if (!TEST_CHECK(result == imap_tests[i].expected))
      {
        TEST_MSG("Expected: %ld", imap_tests[i].expected);
        TEST_MSG("Actual  : %ld", result);
      }
    }
  }
}
