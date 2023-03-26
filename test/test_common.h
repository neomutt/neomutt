/**
 * @file
 * Common code for file tests
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

#ifndef TEST_TEST_COMMON_H
#define TEST_TEST_COMMON_H

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"

struct NeoMutt;

void test_gen_path(char *buf, size_t buflen, const char *fmt);

bool test_neomutt_create (void);
void test_neomutt_destroy(void);

static inline bool test_check_str_eq(const char *expected, const char *actual, const char *file, int lnum)
{
  const bool rc = mutt_str_equal(expected, actual);
  if (!acutest_check_(rc, file, lnum, "test_check_str_eq"))
  {
    TEST_MSG("Expected: %s", expected);
    TEST_MSG("Actual  : %s", actual);
  }

  return rc;
}

#define TEST_CHECK_STR_EQ(expected, actual) test_check_str_eq(expected, actual, __FILE__, __LINE__)

#define LONG_IS_64 (LONG_MAX == 9223372036854775807)

#endif /* TEST_TEST_COMMON_H */
