/**
 * @file
 * Test code for unlink_message()
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

void test_unlink_message(void)
{
  // void unlink_message(struct MuttThread **old, struct MuttThread *cur);

  {
    struct MuttThread cur = { 0 };
    unlink_message(NULL, &cur);
    TEST_CHECK_(1, "unlink_message(NULL, &cur)");
  }

  {
    struct MuttThread *old = NULL;
    unlink_message(&old, NULL);
    TEST_CHECK_(1, "unlink_message(&old, NULL)");
  }
}
