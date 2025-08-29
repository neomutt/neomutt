/**
 * @file
 * Test code for mutt_md5_bytes()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Pietro Cerutti <gahr@gahr.ch>
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
#include <string.h>
#include "mutt/lib.h"
#include "common.h"
#include "test_common.h"

void test_mutt_md5_bytes(void)
{
  // void *mutt_md5_bytes(const void *buffer, size_t len, void *resbuf);

  {
    char buf[32] = { 0 };
    mutt_md5_bytes(NULL, 10, &buf);
    TEST_CHECK_(1, "mutt_md5_bytes(NULL, 10, &buf)");
  }

  {
    char buf[32] = { 0 };
    mutt_md5_bytes(&buf, 10, NULL);
    TEST_CHECK_(1, "mutt_md5_bytes(&buf, 10, NULL)");
  }

  {
    for (size_t i = 0; md5_test_data[i].text; i++)
    {
      struct Md5Ctx ctx = { 0 };
      unsigned char buf[16];
      char digest[33];
      mutt_md5_init_ctx(&ctx);
      mutt_md5_process_bytes(md5_test_data[i].text, strlen(md5_test_data[i].text), &ctx);
      mutt_md5_finish_ctx(&ctx, buf);
      mutt_md5_toascii(buf, digest);
      TEST_CHECK_STR_EQ(digest, md5_test_data[i].hash);
    }
  }
}
