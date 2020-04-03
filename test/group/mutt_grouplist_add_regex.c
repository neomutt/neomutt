/**
 * @file
 * Test code for mutt_grouplist_add_regex()
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

void test_mutt_grouplist_add_regex(void)
{
  // int mutt_grouplist_add_regex(struct GroupList *head, const char *s, int flags, struct Buffer *err);

  {
    struct Buffer err = mutt_buffer_make(0);
    TEST_CHECK(mutt_grouplist_add_regex(NULL, "apple", 0, &err) == -1);
  }

  {
    struct GroupList head = { 0 };
    struct Buffer err = mutt_buffer_make(0);
    TEST_CHECK(mutt_grouplist_add_regex(&head, NULL, 0, &err) == -1);
  }

  {
    struct GroupList head = { 0 };
    TEST_CHECK(mutt_grouplist_add_regex(&head, "apple", 0, NULL) == 0);
  }
}
