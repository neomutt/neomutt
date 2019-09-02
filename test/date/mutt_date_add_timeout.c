/**
 * @file
 * Test code for mutt_date_add_timeout()
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
#include "acutest.h"
#include "config.h"
#include "mutt/mutt.h"

void test_mutt_date_add_timeout(void)
{
  // time_t mutt_date_add_timeout(time_t now, long timeout);

  time_t now = mutt_date_epoch();

  TEST_CHECK(mutt_date_add_timeout(now, -1000) == now);
  TEST_CHECK(mutt_date_add_timeout(now, -1) == now);
  TEST_CHECK(mutt_date_add_timeout(now, 0) == now);
  TEST_CHECK(mutt_date_add_timeout(now, 1) == (now + 1));
  TEST_CHECK(mutt_date_add_timeout(now, 1000) == (now + 1000));

  TEST_CHECK(mutt_date_add_timeout(now, TIME_T_MAX) == TIME_T_MAX);
  TEST_CHECK(mutt_date_add_timeout(TIME_T_MAX - 1, 0) == (TIME_T_MAX - 1));
  TEST_CHECK(mutt_date_add_timeout(TIME_T_MAX - 1, 1) == TIME_T_MAX);
  TEST_CHECK(mutt_date_add_timeout(TIME_T_MAX - 1, 2) == TIME_T_MAX);
}
