/**
 * @file
 * Test code for url_check_scheme()
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
#include "email/lib.h"

void test_url_check_scheme(void)
{
  // enum UrlScheme url_check_scheme(const char *s);

  TEST_CHECK(url_check_scheme(NULL) == U_UNKNOWN);
  TEST_CHECK(url_check_scheme("foobar") == U_UNKNOWN);
  TEST_CHECK(url_check_scheme("foobar:") == U_UNKNOWN);
  TEST_CHECK(url_check_scheme("PoP:") == U_POP);
  TEST_CHECK(url_check_scheme("IMAPS:") == U_IMAPS);
  TEST_CHECK(url_check_scheme("ImApSBAR:") == U_UNKNOWN);
  TEST_CHECK(url_check_scheme("mailto:foo@bar.baz>") == U_UNKNOWN);
}
