/**
 * @file
 * Test code for mutt_ch_iconv_open()
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
#include "mutt/lib.h"

void test_mutt_ch_iconv_open(void)
{
  // iconv_t mutt_ch_iconv_open(const char *tocode, const char *fromcode, uint8_t flags);

  {
    TEST_CHECK(mutt_ch_iconv_open(NULL, "banana", MUTT_ICONV_NO_FLAGS) != NULL);
  }

  {
    TEST_CHECK(mutt_ch_iconv_open("apple", NULL, MUTT_ICONV_NO_FLAGS) != NULL);
  }
}
