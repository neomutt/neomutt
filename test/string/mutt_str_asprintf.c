/**
 * @file
 * Test code for mutt_str_asprintf()
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

void test_mutt_str_asprintf(void)
{
  // int mutt_str_asprintf(char **strp, const char *fmt, ...);

  {
    TEST_CHECK(mutt_str_asprintf(NULL, "dummy") == -1);
  }

  {
    char *ptr = NULL;
    TEST_CHECK(mutt_str_asprintf(&ptr, NULL) == -1);
  }

  {
    TEST_CASE("Empty");
    char *result = NULL;
    TEST_CHECK(mutt_str_asprintf(&result, "") == 0);
    TEST_CHECK(result == NULL);
  }

  {
    TEST_CASE("Static");
    const char *str = "apple";
    char *result = NULL;
    TEST_CHECK(mutt_str_asprintf(&result, str) == 5);
    TEST_CHECK(strcmp(result, str) == 0);
    FREE(&result);
  }

  {
    TEST_CASE("Static big");
    const char *str =
        "apple banana cherry damson elderberry fig guava hawthorn ilama "
        "jackfruit kumquat lemon mango nectarine olive papaya quince raspberry "
        "strawberry tangerine ugli vanilla wolfberry xigua yew ziziphus";
    char *result = NULL;
    TEST_CHECK(mutt_str_asprintf(&result, str) == 195);
    TEST_CHECK(strcmp(result, str) == 0);
    FREE(&result);
  }

  {
    TEST_CASE("Varargs");
    const char *str = "apple";
    const char *expected = "app 1234567 3.1416";
    char *result = NULL;
    TEST_CHECK(mutt_str_asprintf(&result, "%.3s %ld %3.4f", str, 1234567, 3.141592654) == 18);
    TEST_CHECK(strcmp(result, expected) == 0);
    FREE(&result);
  }
}
