/**
 * @file
 * Test code for mutt_md5_finish_ctx()
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

void test_mutt_md5_finish_ctx(void)
{
  // void *mutt_md5_finish_ctx(struct Md5Ctx *md5ctx, void *resbuf);

  {
    char buf[32] = { 0 };
    mutt_md5_finish_ctx(NULL, &buf);
    TEST_CHECK_(1, "mutt_md5_finish_ctx(NULL, &buf)");
  }

  {
    struct Md5Ctx md5ctx = { 0 };
    mutt_md5_finish_ctx(&md5ctx, NULL);
    TEST_CHECK_(1, "mutt_md5_finish_ctx(&md5ctx, NULL)");
  }
}
