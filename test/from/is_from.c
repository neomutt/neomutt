/**
 * @file
 * Test code for is_from()
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
#include "address/lib.h"
#include "email/lib.h"

void test_is_from(void)
{
  // bool is_from(const char *s, char *path, size_t pathlen, time_t *tp);

  {
    char buf[32] = { 0 };
    time_t t = { 0 };
    TEST_CHECK(!is_from(NULL, buf, sizeof(buf), &t));
  }

  {
    time_t t = { 0 };
    TEST_CHECK(!is_from("apple", NULL, 10, &t));
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(!is_from("apple", buf, sizeof(buf), NULL));
  }
}
