/**
 * @file
 * Test code for mutt_file_copy_bytes()
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
#include <stdio.h>
#include "mutt/lib.h"

void test_mutt_file_copy_bytes(void)
{
  // int mutt_file_copy_bytes(FILE *fp_in, FILE *fp_out, size_t size);

  {
    FILE *fp = fopen("/dev/null", "r");
    TEST_CHECK(mutt_file_copy_bytes(NULL, fp, 10) != 0);
    fclose(fp);
  }

  {
    FILE *fp = fopen("/dev/null", "r");
    TEST_CHECK(mutt_file_copy_bytes(fp, NULL, 10) != 0);
    fclose(fp);
  }
}
