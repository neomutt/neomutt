/**
 * @file
 * Test code for mutt_str_strcasestr()
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

void test_mutt_str_strcasestr(void)
{
  // const char *mutt_str_strcasestr(const char *haystack, const char *needle);

  {
    TEST_CHECK(mutt_str_strcasestr(NULL, "apple") == NULL);
  }

  {
    TEST_CHECK(mutt_str_strcasestr("apple", NULL) == NULL);
  }

  {
    TEST_CHECK(mutt_str_strcasestr(NULL, NULL) == NULL);
  }

  char *haystack_same_size = "hello";
  char *haystack_larger = "hello, world!";
  char *haystack_smaller = "heck";
  char *haystack_mid = "test! hello, world";
  char *haystack_end = ", world! hello";

  char *empty = "";

  const char *needle = "hEllo";
  const char *needle_lower = "hello";
  const char *nonexistent = "goodbye";

  { // Check NULL conditions
    const char *retval1 = mutt_str_strcasestr(NULL, NULL);
    const char *retval2 = mutt_str_strcasestr(NULL, needle);
    const char *retval3 = mutt_str_strcasestr(haystack_same_size, NULL);

    TEST_CHECK_(retval1 == NULL, "Expected: %s, Actual %s", NULL, retval1);
    TEST_CHECK_(retval2 == NULL, "Expected: %s, Actual %s", NULL, retval2);
    TEST_CHECK_(retval3 == NULL, "Expected: %s, Actual %s", NULL, retval3);
  }

  { // Check empty strings
    const char *retval1 = mutt_str_strcasestr(empty, empty);
    const char *retval2 = mutt_str_strcasestr(empty, needle);
    const char *retval3 = mutt_str_strcasestr(haystack_same_size, empty);

    const char *empty_expected = strstr(empty, empty);

    TEST_CHECK_(retval1 == empty_expected, "Expected: %s, Actual %s", "", retval1);
    TEST_CHECK_(retval2 == NULL, "Expected: %s, Actual %s", NULL, retval2);
    TEST_CHECK_(retval3 == haystack_same_size, "Expected: %s, Actual %s",
                haystack_same_size, retval3);
  }

  { // Check instance where needle is not in haystack.
    const char *retval1 = mutt_str_strcasestr(haystack_same_size, nonexistent);
    const char *retval2 = mutt_str_strcasestr(haystack_smaller, nonexistent);
    const char *retval3 = mutt_str_strcasestr(haystack_larger, nonexistent);

    TEST_CHECK_(retval1 == NULL, "Expected: %s, Actual %s", NULL, retval1);
    TEST_CHECK_(retval2 == NULL, "Expected: %s, Actual %s", NULL, retval2);
    TEST_CHECK_(retval3 == NULL, "Expected: %s, Actual %s", NULL, retval3);
  }

  { // Check instance haystack is the same length as the needle and needle exists.
    const char *retval = mutt_str_strcasestr(haystack_same_size, needle);

    TEST_CHECK_(retval == haystack_same_size, "Expected: %s, Actual: %s",
                haystack_same_size, retval);
  }

  { // Check instance haystack is larger and needle exists.
    const char *retval = mutt_str_strcasestr(haystack_larger, needle);
    const char *expected = strstr(haystack_larger, needle_lower);

    TEST_CHECK_(retval == expected, "Expected: %s, Actual: %s", haystack_larger, retval);
  }

  { // Check instance where word is in the middle
    const char *retval = mutt_str_strcasestr(haystack_mid, needle);
    const char *expected = strstr(haystack_mid, needle_lower);

    TEST_CHECK_(retval == expected, "Expected: %s, Actual: %s", expected, retval);
  }

  { // Check instance where needle is at the end,
    const char *retval = mutt_str_strcasestr(haystack_end, needle);
    const char *expected = strstr(haystack_end, needle_lower);

    TEST_CHECK_(retval == expected, "Expected: %s, Actual: %s", expected, retval);
  }

  { // Check instance where haystack is smaller than needle.
    const char *retval = mutt_str_strcasestr(haystack_smaller, needle);

    TEST_CHECK_(retval == NULL, "Expected: %s, Actual: %s", NULL, retval);
  }
}
