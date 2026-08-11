/**
 * @file
 * Test code for bg_job_start()
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
#include "background/lib.h"
#include "test_common.h"

void test_bg_job_start(void)
{
  // int bg_job_start(const char *cmd)

  {
    const int slot = bg_job_start("exit 0");
    TEST_CHECK(slot >= 0);
    TEST_CHECK_NUM_EQ(bg_job_exit_code(slot), -1); // still running

    int reaped = 0;
    for (int i = 0; (i < 10) && (reaped == 0); i++)
    {
      sleep(1);
      reaped = bg_reap();
    }
    TEST_CHECK_NUM_EQ(reaped, 1);
    TEST_CHECK_NUM_EQ(bg_job_exit_code(slot), 0);
  }

  {
    // Finished jobs stay in the table: 9 more fit, the 10th fails
    for (int i = 0; i < 9; i++)
      TEST_CHECK(bg_job_start("exec sleep 30") >= 0);
    TEST_CHECK_NUM_EQ(bg_job_start("exit 0"), -1);
  }

  TEST_CHECK_NUM_EQ(bg_job_exit_code(-1), -1);
  TEST_CHECK_NUM_EQ(bg_job_exit_code(99), -1);

  bg_cleanup();
}
