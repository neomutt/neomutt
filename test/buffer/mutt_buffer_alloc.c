/**
 * @file
 * Test code for mutt_buffer_alloc()
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
#include <stdbool.h>
#include "mutt/mutt.h"

void test_mutt_buffer_alloc(void)
{
  // struct Buffer *mutt_buffer_alloc(size_t size);

  size_t alloc_tests[] = { 0, 42, 1048576 };

  for (size_t i = 0; i < mutt_array_size(alloc_tests); i++)
  {
    struct Buffer *buf = NULL;
    TEST_CASE_("%lu", alloc_tests[i]);
    buf = mutt_buffer_alloc(alloc_tests[i]);
    TEST_CHECK(buf != NULL);
    mutt_buffer_free(&buf);
  }
}
