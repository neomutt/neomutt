/**
 * @file
 * Test code for mutt_str_getenv()
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

void test_mutt_str_getenv(void)
{
  // const char *mutt_str_getenv(const char *name);

  {
    TEST_CHECK(mutt_str_getenv(NULL) == NULL);
  }

  {
    const char *name = "NEOMUTT_TEST1";
    const char *value = "hello";
    setenv(name, value, 1);
    TEST_CASE(name);

    const char *result = mutt_str_getenv(name);
    TEST_CHECK(strcmp(result, value) == 0);
    unsetenv(name);
  }

  {
    const char *name = "NEOMUTT_TEST2";
    const char *value = "";
    setenv(name, value, 1);
    TEST_CASE(name);

    const char *result = mutt_str_getenv(name);
    TEST_CHECK(result == NULL);
    unsetenv(name);
  }

  {
    const char *name = "NEOMUTT_TEST3";
    TEST_CASE(name);

    const char *result = mutt_str_getenv(name);
    TEST_CHECK(result == NULL);
  }
}
