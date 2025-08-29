/**
 * @file
 * Test code for mutt_str_lower()
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
#include "mutt/lib.h"
#include "test_common.h"

void test_mutt_str_lower(void)
{
  // char *mutt_str_lower(char *s);

  {
    TEST_CHECK(mutt_str_lower(NULL) == NULL);
  }

  {
    char buf[64] = "";
    mutt_str_lower(buf);
    TEST_CHECK_STR_EQ(buf, "");
  }

  {
    char buf[64] = "apple";
    mutt_str_lower(buf);
    TEST_CHECK_STR_EQ(buf, "apple");
  }

  {
    char buf[64] = "aPPLe";
    mutt_str_lower(buf);
    TEST_CHECK_STR_EQ(buf, "apple");
  }
}
