/**
 * @file
 * Test code for mutt_regexlist_add()
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

void test_mutt_regexlist_add(void)
{
  // int mutt_regexlist_add(struct RegexList *rl, const char *str, int flags, struct Buffer *err);

  {
    struct Buffer buf = mutt_buffer_make(0);
    TEST_CHECK(mutt_regexlist_add(NULL, "apple", 0, &buf) == 0);
  }

  {
    struct RegexList regexlist = { 0 };
    struct Buffer buf = mutt_buffer_make(0);
    TEST_CHECK(mutt_regexlist_add(&regexlist, NULL, 0, &buf) == 0);
  }

  {
    struct RegexList regexlist = STAILQ_HEAD_INITIALIZER(regexlist);
    TEST_CHECK(mutt_regexlist_add(&regexlist, "apple", 0, NULL) == 0);
    mutt_regexlist_free(&regexlist);
  }
}
