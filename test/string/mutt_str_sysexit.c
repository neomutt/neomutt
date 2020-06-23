/**
 * @file
 * Test code for mutt_str_sysexit()
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
#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#endif

struct TestValue
{
  int err_num;        ///< Error number to lookup
  const char *result; ///< Expected result string
};

// clang-format off
static const struct TestValue tests[] = {
#ifdef EX_NOUSER
  { 0xff & EX_NOUSER,      "User unknown."            },
#endif
#ifdef EX_NOHOST
  { 0xff & EX_NOHOST,      "Host unknown."            },
#endif
#ifdef EX_UNAVAILABLE
  { 0xff & EX_UNAVAILABLE, "Service unavailable."     },
#endif
#ifdef EX_IOERR
  { 0xff & EX_IOERR,       "I/O error."               },
#endif
#ifdef EX_NOPERM
  { 0xff & EX_NOPERM,      "Insufficient permission." },
#endif
  { 255,                   NULL                       },
  { -1,                    NULL                       },
};
// clang-format on

void test_mutt_str_sysexit(void)
{
  // const char *mutt_str_sysexit(int e);

  const char *result = NULL;

  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    TEST_MSG("Testing %d, expecting '%s'\n", tests[i].err_num, NONULL(tests[i].result));
    result = mutt_str_sysexit(tests[i].err_num);

    if (!TEST_CHECK(mutt_str_equal(result, tests[i].result)))
    {
      TEST_MSG("Expected: '%s', Got: '%s'\n", result, NONULL(tests[i].result));
      return;
    }
  }
}
