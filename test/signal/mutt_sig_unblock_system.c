/**
 * @file
 * Test code for mutt_sig_unblock_system()
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
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
#include "mutt/lib.h"

void test_mutt_sig_unblock_system(void)
{
  // void mutt_sig_unblock_system(bool catch);

  {
    mutt_sig_unblock();
    mutt_sig_unblock_system(false);
    TEST_CHECK_(1, "mutt_sig_unblock_system(false)");
  }

  {
    mutt_sig_unblock();
    mutt_sig_unblock();
    mutt_sig_unblock_system(false);
    TEST_CHECK_(1, "mutt_sig_unblock_system(false)");
  }

  {
    mutt_sig_block();
    mutt_sig_unblock_system(false);
    TEST_CHECK_(1, "mutt_sig_unblock_system(false)");
  }
}
