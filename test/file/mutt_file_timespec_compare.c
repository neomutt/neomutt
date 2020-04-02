/**
 * @file
 * Test code for mutt_file_timespec_compare()
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

void test_mutt_file_timespec_compare(void)
{
  // int mutt_file_timespec_compare(struct timespec *a, struct timespec *b);

  {
    struct timespec timespec = { 0 };
    TEST_CHECK(mutt_file_timespec_compare(NULL, &timespec) == 0);
  }

  {
    struct timespec timespec = { 0 };
    TEST_CHECK(mutt_file_timespec_compare(&timespec, NULL) == 0);
  }
}
