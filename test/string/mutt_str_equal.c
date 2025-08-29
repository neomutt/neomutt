/**
 * @file
 * Test code for mutt_str_equal()
 *
 * @authors
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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

void test_mutt_str_equal(void)
{
  // bool mutt_str_equal(const char *a, const char *b);

  TEST_CHECK(!mutt_str_equal(NULL, "apple"));
  TEST_CHECK(!mutt_str_equal("apple", NULL));
  TEST_CHECK(mutt_str_equal(NULL, NULL));

  TEST_CHECK(mutt_str_equal("", ""));
  TEST_CHECK(mutt_str_equal("apple", "apple"));
  TEST_CHECK(!mutt_str_equal("apple", "APPLE"));
  TEST_CHECK(!mutt_str_equal("apple", "apple2"));
  TEST_CHECK(!mutt_str_equal("apple1", "apple"));
}
