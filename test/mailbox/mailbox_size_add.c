/**
 * @file
 * Test code for mailbox_size_add()
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
#include "core/lib.h"

void test_mailbox_size_add(void)
{
  // void mailbox_size_add(struct Mailbox *m, const struct Email *e);

  {
    struct Mailbox m = { 0 };
    mailbox_size_add(&m, NULL);
    TEST_CHECK_(1, "mailbox_size_add(&m, NULL)");
  }
}
