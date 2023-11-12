/**
 * @file
 * Test code for mutt_date_make_date()
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
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"

void test_mutt_date_make_date(void)
{
  // void mutt_date_make_date(struct Buffer *buf);

  bool local = true;
  do
  {
    {
      mutt_date_make_date(NULL, local);
      TEST_CHECK_(1, "mutt_date_make_date(NULL, %s)", local ? "true" : "false");
    }

    {
      struct Buffer *buf = buf_pool_get();
      mutt_date_make_date(buf, local);
      TEST_CHECK(buf_len(buf) != 0);
      TEST_MSG("%s", buf);
      buf_pool_release(&buf);
    }
    local = !local;
  } while (!local); // test the same with local == false
}
