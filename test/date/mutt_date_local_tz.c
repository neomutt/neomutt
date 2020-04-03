/**
 * @file
 * Test code for mutt_date_local_tz()
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
#include <stdlib.h>
#include "mutt/lib.h"

void test_mutt_date_local_tz(void)
{
  // time_t mutt_date_local_tz(time_t t);

  TEST_CHECK(mutt_date_local_tz(TIME_T_MIN) == 0);
  TEST_CHECK(mutt_date_local_tz(TIME_T_MAX) == 0);

  {
    time_t seconds = mutt_date_local_tz(0);
    TEST_MSG("seconds = %ld", seconds);
    TEST_CHECK(seconds < 86400);
  }

  {
    // December
    time_t seconds = mutt_date_local_tz(977745600);
    TEST_MSG("seconds = %ld", seconds);
    TEST_CHECK(seconds < 86400);
  }

  {
    // June
    time_t seconds = mutt_date_local_tz(961930800);
    TEST_MSG("seconds = %ld", seconds);
    TEST_CHECK(seconds < 86400);
  }
}
