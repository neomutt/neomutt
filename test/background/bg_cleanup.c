/**
 * @file
 * Test code for bg_cleanup()
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

void test_bg_cleanup(void)
{
  // void bg_cleanup(void)

  {
    const int slot = bg_job_start("exec sleep 30");
    TEST_CHECK(slot >= 0);
    TEST_CHECK_NUM_EQ(bg_job_exit_code(slot), -1); // still running

    bg_cleanup();
    TEST_CHECK_NUM_EQ(bg_job_exit_code(slot), 0); // slot released

    // The released slot can be reused
    TEST_CHECK_NUM_EQ(bg_job_start("exit 0"), slot);
  }

  bg_cleanup(); // idempotent
}
