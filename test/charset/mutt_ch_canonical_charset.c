/**
 * @file
 * Test code for mutt_ch_canonical_charset()
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

void test_mutt_ch_canonical_charset(void)
{
  // void mutt_ch_canonical_charset(char *buf, size_t buflen, const char *name);

  {
    mutt_ch_canonical_charset(NULL, 10, "apple");
    TEST_CHECK_(1, "mutt_ch_canonical_charset(NULL, 10, \"apple\")");
  }

  {
    char buf[32] = { 0 };
    mutt_ch_canonical_charset(buf, sizeof(buf), NULL);
    TEST_CHECK_(1, "mutt_ch_canonical_charset(&buf, sizeof(buf), NULL)");
  }
}
