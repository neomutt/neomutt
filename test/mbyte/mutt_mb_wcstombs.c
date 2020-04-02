/**
 * @file
 * Test code for mutt_mb_wcstombs()
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

void test_mutt_mb_wcstombs(void)
{
  // void mutt_mb_wcstombs(char *dest, size_t dlen, const wchar_t *src, size_t slen);

  {
    wchar_t src[32] = L"apple";
    mutt_mb_wcstombs(NULL, 10, src, 3);
    TEST_CHECK_(1, "mutt_mb_wcstombs(&buf, sizeof(buf), src, 3)");
  }

  {
    char buf[32] = { 0 };
    mutt_mb_wcstombs(buf, sizeof(buf), NULL, 3);
    TEST_CHECK_(1, "mutt_mb_wcstombs(&buf, sizeof(buf), NULL, 3)");
  }
}
