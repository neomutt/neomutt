/**
 * @file
 * Test code for thread_hash_destructor()
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
#include "address/lib.h"
#include "email/lib.h"

void test_thread_hash_destructor(void)
{
  // void thread_hash_destructor(int type, void *obj, intptr_t data);

  {
    thread_hash_destructor(0, NULL, (intptr_t) "apple");
    TEST_CHECK_(1, "thread_hash_destructor(0, NULL, \"apple\")");
  }

  {
    struct Address *address = mutt_mem_calloc(1, sizeof(*address));
    thread_hash_destructor(0, address, 0);
    TEST_CHECK_(1, "thread_hash_destructor(0, &address, NULL)");
  }
}
