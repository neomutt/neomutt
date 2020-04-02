/**
 * @file
 * Test code for mutt_str_adjust()
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

void test_mutt_str_adjust(void)
{
  // void mutt_str_adjust(char **p);

  {
    mutt_str_adjust(NULL);
    TEST_CHECK_(1, "mutt_str_adjust(NULL)");
  }

  {
    char *ptr = NULL;
    mutt_str_adjust(&ptr);
    TEST_CHECK_(1, "mutt_str_adjust(&ptr)");
  }

  {
    char *ptr = strdup("");
    mutt_str_adjust(&ptr);
    TEST_CHECK_(1, "mutt_str_adjust(&ptr)");
    FREE(&ptr);
  }

  {
    const int len = 15;
    char *ptr = strdup("the quick brown fox jumps over a lazy dog");
    ptr[len] = 0;
    mutt_str_adjust(&ptr);
    TEST_CHECK_(1, "mutt_str_adjust(&ptr)");
    TEST_CHECK(strlen(ptr) == len);
    FREE(&ptr);
  }
}
