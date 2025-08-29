/**
 * @file
 * Test code for mutt_date_make_imap()
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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
#include <stdlib.h>
#include <time.h>
#include "mutt/lib.h"

void test_mutt_date_make_imap(void)
{
  // int mutt_date_make_imap(struct Buffer *buf, time_t timestamp);

  {
    TEST_CHECK(mutt_date_make_imap(NULL, 0) != 0);
  }

  {
    struct Buffer *buf = buf_pool_get();
    time_t t = 961930800;
    TEST_CHECK(mutt_date_make_imap(buf, t) > 0);
    TEST_MSG(buf_string(buf));
    bool result = (mutt_str_equal(buf_string(buf), "25-Jun-2000 12:00:00 +0100")) || // Expected result...
                  (mutt_str_equal(buf_string(buf), "25-Jun-2000 11:00:00 +0000")); // but Travis seems to have locale problems
    TEST_CHECK(result == true);
    buf_pool_release(&buf);
  }
}
