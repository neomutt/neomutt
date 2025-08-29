/**
 * @file
 * Test code for neomutt_mailboxlist_get_all()
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
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"

void test_neomutt_mailboxlist_get_all(void)
{
  // size_t neomutt_mailboxlist_get_all(struct MailboxList *head, struct NeoMutt *n, enum MailboxType magic);

  {
    struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
    size_t count = neomutt_mailboxlist_get_all(&ml, NULL, MUTT_MAILDIR);
    TEST_CHECK(count == 0);
    TEST_CHECK(STAILQ_EMPTY(&ml) == true);
  }
}
