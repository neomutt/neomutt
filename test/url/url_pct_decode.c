/**
 * @file
 * Test code for url_pct_decode()
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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
#include <string.h>
#include "email/lib.h"
#include "test_common.h"

void test_url_pct_decode(void)
{
  // int url_pct_decode(char *s);

  {
    TEST_CHECK(url_pct_decode(NULL) != 0);
  }

  {
    char s[] = "Hello%20world";
    TEST_CHECK(url_pct_decode(s) == 0);
    TEST_CHECK_STR_EQ(s, "Hello world");
  }
}
