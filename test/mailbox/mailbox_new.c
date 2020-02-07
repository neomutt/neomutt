/**
 * @file
 * Test code for mailbox_new()
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

void test_mailbox_new(void)
{
  // struct Mailbox *mailbox_new(void);

  {
    struct Path *p = mutt_path_new();
    p->orig = mutt_str_strdup("/home/mutt/mail");
    struct Mailbox *m = mailbox_new(p);
    TEST_CHECK(m != NULL);

    TEST_CHECK(mailbox_path(m) != NULL);

    mailbox_free(&m);
  }
}
