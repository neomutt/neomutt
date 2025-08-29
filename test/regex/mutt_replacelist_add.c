/**
 * @file
 * Test code for mutt_replacelist_add()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
#include <stddef.h>
#include "mutt/lib.h"

void test_mutt_replacelist_add(void)
{
  // int mutt_replacelist_add(struct ReplaceList *rl, const char *pat, const char *templ, struct Buffer *err);

  {
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK(mutt_replacelist_add(NULL, "apple", "banana", buf) == 0);
    buf_pool_release(&buf);
  }

  {
    struct ReplaceList replacelist = { 0 };
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK(mutt_replacelist_add(&replacelist, NULL, "banana", buf) == 0);
    buf_pool_release(&buf);
  }

  {
    struct ReplaceList replacelist = { 0 };
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK(mutt_replacelist_add(&replacelist, "apple", NULL, buf) == 0);
    buf_pool_release(&buf);
  }

  {
    struct ReplaceList replacelist = STAILQ_HEAD_INITIALIZER(replacelist);
    TEST_CHECK(mutt_replacelist_add(&replacelist, "apple", "banana", NULL) == 0);
    mutt_replacelist_free(&replacelist);
  }
}
