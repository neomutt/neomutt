/**
 * @file
 * Test code for mailbox_free()
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
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"

void mdata_free(void **ptr)
{
  FREE(ptr);
}

void test_mailbox_free(void)
{
  // void mailbox_free(struct Mailbox **ptr);

  {
    mailbox_free(NULL);
    TEST_CHECK_(1, "mailbox_free(NULL)");
  }

  {
    struct Mailbox *m = NULL;
    mailbox_free(&m);
    TEST_CHECK_(1, "mailbox_free(&m)");
  }

  {
    struct Mailbox *m = MUTT_MEM_CALLOC(1, struct Mailbox);
    mailbox_free(&m);
    TEST_CHECK_(1, "mailbox_free(&m)");
  }

  {
    struct Mailbox *m = MUTT_MEM_CALLOC(1, struct Mailbox);
    m->mdata = mutt_mem_calloc(32, 1);
    m->mdata_free = mdata_free;

    mailbox_free(&m);
    TEST_CHECK_(1, "mailbox_free(&m)");
  }
}
