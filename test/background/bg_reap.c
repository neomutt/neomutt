/**
 * @file
 * Test code for bg_reap()
 *
 * @authors
 * Copyright (C) 2026 Reza Jelveh
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
#include <unistd.h>
#include "background.h"
#include "test_common.h"

void test_bg_reap(void)
{
  // int bg_reap(void)

  {
    const int slot = bg_job_start("exit 5");
    TEST_CHECK(slot >= 0);

    int reaped = 0;
    for (int i = 0; (i < 10) && (reaped == 0); i++)
    {
      sleep(1);
      reaped = bg_reap();
    }
    TEST_CHECK_NUM_EQ(reaped, 1);
    TEST_CHECK_NUM_EQ(bg_job_exit_code(slot), 5);
    TEST_CHECK_NUM_EQ(bg_reap(), 0); // nothing left to reap
  }

  {
    const int slot = bg_job_start("kill -9 $$");
    TEST_CHECK(slot >= 0);

    int reaped = 0;
    for (int i = 0; (i < 10) && (reaped == 0); i++)
    {
      sleep(1);
      reaped = bg_reap();
    }
    TEST_CHECK_NUM_EQ(reaped, 1);
    TEST_CHECK_NUM_EQ(bg_job_exit_code(slot), -1); // killed by a signal
  }

  bg_cleanup();
}
