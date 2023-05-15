/**
 * @file
 * Test code for mutt_str_sep()
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
#include <stddef.h>
#include "mutt/lib.h"
#include "test_common.h"

void test_mutt_str_sep(void)
{
  // char *mutt_str_sep(char **stringp, const char *delim);

  char *input = "apple,banana:cherry";
  char *empty = "";
  const char *delim = ",:";

  {
    // degenerate tests
    TEST_CHECK(mutt_str_sep(NULL, delim) == NULL);
    TEST_CHECK(mutt_str_sep(&empty, NULL) == NULL);
    TEST_CHECK(mutt_str_sep(&input, NULL) == NULL);
  }

  {
    char *copy = strdup(input);
    char *result = mutt_str_sep(&copy, ",:");
    TEST_CHECK_STR_EQ(result, "apple");
    FREE(&result);
  }
}
