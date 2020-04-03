/**
 * @file
 * Test code for mutt_hist_search()
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
#include "history/lib.h"

void test_mutt_hist_search(void)
{
  // int mutt_hist_search(const char *search_buf, enum HistoryClass hclass, char **matches);

  {
    char *matches = NULL;
    TEST_CHECK(mutt_hist_search(NULL, 0, &matches) == 0);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_hist_search(buf, 0, NULL) == 0);
  }
}
