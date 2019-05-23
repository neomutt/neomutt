/**
 * @file
 * Test code for mutt_grouplist_add()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
#include "mutt/mutt.h"
#include "address/lib.h"

void test_mutt_grouplist_add(void)
{
  // void mutt_grouplist_add(struct GroupList *head, struct Group *group);

  {
    struct Group group;
    mutt_grouplist_add(NULL, &group);
    TEST_CHECK_(1, "mutt_grouplist_add(NULL, &group)");
  }

  {
    struct GroupList head = { 0 };
    mutt_grouplist_add(&head, NULL);
    TEST_CHECK_(1, "mutt_grouplist_add(&head, NULL)");
  }
}
