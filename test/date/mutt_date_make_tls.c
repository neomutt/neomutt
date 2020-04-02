/**
 * @file
 * Test code for mutt_date_make_tls()
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

void test_mutt_date_make_tls(void)
{
  // int mutt_date_make_tls(char *buf, size_t buflen, time_t timestamp);

  {
    TEST_CHECK(mutt_date_make_tls(NULL, 10, 0) != 0);
  }

  {
    char buf[64] = { 0 };
    time_t t = 961930800;
    TEST_CHECK(mutt_date_make_tls(buf, sizeof(buf), t) > 0);
    TEST_CHECK(strcmp(buf, "Sun, 25 Jun 2000 11:00:00 UTC") == 0);
  }
}
