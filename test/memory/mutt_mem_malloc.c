/**
 * @file
 * Test code for mutt_mem_malloc()
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

bool exit_called = false;

void mutt_exit(int code)
{
  exit_called = true;
}

void test_mutt_mem_malloc(void)
{
  // void *mutt_mem_malloc(size_t size);

  MuttLogger = log_disp_null;
  size_t big = ~0;
  void *ptr = mutt_mem_malloc(big);
  TEST_CHECK(ptr == NULL);
  TEST_CHECK(exit_called == true);

  FREE(&ptr);
}
