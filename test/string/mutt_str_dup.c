/**
 * @file
 * Test code for mutt_str_dup()
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

void test_mutt_str_dup(void)
{
  // char *mutt_str_dup(const char *str);

  {
    TEST_CHECK(mutt_str_dup(NULL) == NULL);
  }

  {
    TEST_CHECK(mutt_str_dup("") == NULL);
  }

  {
    char *str = "apple";
    char *result = mutt_str_dup(str);
    TEST_CHECK(result != NULL);
    TEST_CHECK(result != str);
    TEST_CHECK(strcmp(result, str) == 0);
    FREE(&result);
  }
}
