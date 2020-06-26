/**
 * @file
 * Test code for is_from()
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
#include "address/lib.h"
#include "email/lib.h"

struct IsFromTest
{
  const char *source; // source From line to parse
  bool valid;         // expected validity
  const char *path;   // expected return-path
  time_t epoch;       // expected time in seconds since Epoch
};

// clang-format off
static struct IsFromTest test[] = {
  {
    NULL,
    false
  },
  {
    "apple",
    false
  },
  {
    "From god@heaven.af.mil Sat Jan  3 01:05:34 1996",
    true,
    "god@heaven.af.mil",
    820631134
  },
  {
    "From Sat Jan  3 01:05:34 1996",
    true,
    NULL,
    820631134
  },
  {
    "From god at heaven.af.mil Sat Jan  3 01:05:34 1996",
    true,
    "god at heaven.af.mil",
    820631134
  },
  {
    "From foo@bar.baz SaT Jan  3 01:05:34 1996",
    true,
    "foo@bar.baz",
    820631134
  },
  {
    "From Sat Sat Jan  3 01:05:34 1996",
    true,
    "Sat",
    820631134
  },
  {
    "From god@heaven.af.mil Sat Jan  3 01:05 1996",
    true,
    "god@heaven.af.mil",
    820631100
  },
  {
    "From god@heaven.af.mil Sat Jan  3 01:05:26 96",
    true,
    "god@heaven.af.mil",
    820631126
  },
  {
    "From god@heaven.af.mil Fri Apr 17 01:05:26 20",
    true,
    "god@heaven.af.mil",
    1587085526
  },
  {
    "From god@heaven.af.mil Sat Jan  3 01:05:34 MET DST 1996",
    true,
    "god@heaven.af.mil",
    820631134
  },
};

// clang-format on

void test_is_from(void)
{
  char path[128];
  time_t epoch;

  for (size_t i = 0; i < mutt_array_size(test); i++)
  {
    const struct IsFromTest *t = &test[i];
    TEST_CASE(t->source);
    bool valid = is_from(t->source, path, sizeof(path), &epoch);
    if (!TEST_CHECK(t->valid == valid))
    {
      TEST_MSG("Expected: %svalid", t->valid ? "" : "not ");
      TEST_MSG("Actual  : %svalid", valid ? "" : "not ");
    }

    if (!valid)
      continue;

    if (!TEST_CHECK(mutt_str_equal(t->path, path)))
    {
      TEST_MSG("Expected: %s", t->path);
      TEST_MSG("Actual  : %s", path);
    }

    if (!TEST_CHECK(t->epoch == epoch))
    {
      TEST_MSG("Expected: %zu", t->epoch);
      TEST_MSG("Actual  : %zu", epoch);
    }
  }
}
