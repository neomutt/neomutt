/**
 * @file
 * Test code for mutt_ch_fgetconv_open()
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

void test_mutt_ch_fgetconv_open(void)
{
  // struct FgetConv *mutt_ch_fgetconv_open(FILE *fp, const char *from, const char *to, int flags);

  {
    struct FgetConv *conv = NULL;
    TEST_CHECK((conv = mutt_ch_fgetconv_open(NULL, "apple", "banana", 0)) != NULL);
    mutt_ch_fgetconv_close(&conv);
  }

  {
    FILE fp = { 0 };
    struct FgetConv *conv = NULL;
    TEST_CHECK((conv = mutt_ch_fgetconv_open(&fp, NULL, "banana", 0)) != NULL);
    mutt_ch_fgetconv_close(&conv);
  }

  {
    FILE fp = { 0 };
    struct FgetConv *conv = NULL;
    TEST_CHECK((conv = mutt_ch_fgetconv_open(&fp, "apple", NULL, 0)) != NULL);
    mutt_ch_fgetconv_close(&conv);
  }
}
