/**
 * @file
 * Test code for url_tostring()
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

void test_url_tostring(void)
{
  // int url_tostring(struct Url *u, char *dest, size_t len, int flags);

  {
    char buf[32] = { 0 };
    TEST_CHECK(url_tostring(NULL, buf, sizeof(buf), 0) != 0);
  }

  {
    struct Url url = { 0 };
    TEST_CHECK(url_tostring(&url, NULL, 10, 0) != 0);
  }
}
