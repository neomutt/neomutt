/**
 * @file
 * Test code for mutt_date_is_day_name()
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

void test_mutt_date_is_day_name(void)
{
  // bool mutt_date_is_day_name(const char *s);

  TEST_CHECK(mutt_date_is_day_name(NULL) == false);
  TEST_CHECK(mutt_date_is_day_name("mo") == false);
  TEST_CHECK(mutt_date_is_day_name("Mon") == false);
  TEST_CHECK(mutt_date_is_day_name("Dec ") == false);
  TEST_CHECK(mutt_date_is_day_name("Tuesday") == false);

  TEST_CHECK(mutt_date_is_day_name("Mon ") == true);
  TEST_CHECK(mutt_date_is_day_name("TUE ") == true);
}
