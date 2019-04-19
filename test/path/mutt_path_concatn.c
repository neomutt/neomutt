/**
 * @file
 * Test code for mutt_path_concatn()
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
#include "acutest.h"
#include "config.h"
#include "mutt/mutt.h"

void test_mutt_path_concatn(void)
{
  // char *mutt_path_concatn(char *dst, size_t dstlen, const char *dir, size_t dirlen, const char *fname, size_t fnamelen);

  {
    TEST_CHECK(mutt_path_concatn(NULL, 10, "apple", 5, "banana", 6) == NULL);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_path_concatn(buf, sizeof(buf), NULL, 5, "banana", 6) == NULL);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_path_concatn(buf, sizeof(buf), "apple", 5, NULL, 6) == NULL);
  }
}
