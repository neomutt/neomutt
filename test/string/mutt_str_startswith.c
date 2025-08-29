/**
 * @file
 * Test code for mutt_str_startswith()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
#include <stddef.h>
#include "mutt/lib.h"

void test_mutt_str_startswith(void)
{
  // size_t mutt_str_startswith(const char *str, const char *prefix, enum CaseSensitivity cs);

  TEST_CHECK(mutt_str_startswith(NULL, "apple") == 0);
  TEST_CHECK(mutt_str_startswith("apple", NULL) == 0);

  TEST_CHECK(mutt_str_startswith("", "apple") == 0);
  TEST_CHECK(mutt_str_startswith("apple", "") == 0);

  TEST_CHECK(mutt_str_startswith("applebanana", "apple") == 5);
  TEST_CHECK(mutt_str_startswith("APPLEbanana", "apple") == 0);

  TEST_CHECK(mutt_istr_startswith("APPLEbanana", "apple") == 5);
  TEST_CHECK(mutt_istr_startswith("GUAVAbanana", "apple") == 0);
}
