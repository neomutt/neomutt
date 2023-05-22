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
#include "config.h"
#include "mutt/lib.h"

void test_gen_path(char *buf, size_t buflen, const char *fmt);

bool test_neomutt_create (void);
void test_neomutt_destroy(void);

static inline bool test_check_str_eq(const char *actual, const char *expected, const char *file, int lnum)
{
  const bool rc = mutt_str_equal(actual, expected);
  if (!acutest_check_(rc, file, lnum, "test_check_str_eq"))
  {
    TEST_MSG("Expected : %s", expected);
    TEST_MSG("Actual   : %s", actual);
  }

  return rc;
}

static inline FILE *test_make_file_with_contents(char *contents, size_t len)
{
  FILE *fp = NULL;
#ifdef USE_FMEMOPEN
  fp = fmemopen(contents, len, "r");
#else
  fp = tmpfile();
  if (fp)
  {
    fwrite(contents, len, 1, fp);
    rewind(fp);
  }
#endif
  return fp;
}

#define TEST_CHECK_STR_EQ(actual, expected) test_check_str_eq(actual, expected, __FILE__, __LINE__)

#define LONG_IS_64 (LONG_MAX == 9223372036854775807)

#endif /* TEST_TEST_COMMON_H */
