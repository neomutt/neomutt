/**
 * @file
 * Test code for mutt_ch_convert_string()
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

void test_mutt_ch_convert_string(void)
{
  // int mutt_ch_convert_string(char **ps, const char *from, const char *to, int flags);

  {
    TEST_CHECK(mutt_ch_convert_string(NULL, "apple", "banana", 0) != 0);
  }

  {
    char *ps = NULL;
    TEST_CHECK(mutt_ch_convert_string(&ps, "apple", "banana", 0) == 0);
  }

  {
    char *ps = strdup("apple");
    TEST_CHECK(mutt_ch_convert_string(&ps, NULL, "banana", 0) != 0);
    free(ps);
  }

  {
    char *ps = strdup("apple");
    TEST_CHECK(mutt_ch_convert_string(&ps, "apple", NULL, 0) != 0);
    free(ps);
  }
}
