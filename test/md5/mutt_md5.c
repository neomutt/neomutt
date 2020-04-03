/**
 * @file
 * Test code for mutt_md5()
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
#include "common.h"

void test_mutt_md5(void)
{
  // void *mutt_md5(const char *str, void *buf);

  {
    char buf[32] = { 0 };
    TEST_CHECK(!mutt_md5(NULL, &buf));
  }

  {
    TEST_CHECK(!mutt_md5("apple", NULL));
  }

  {
    for (size_t i = 0; md5_test_data[i].text; i++)
    {
      unsigned char buf[16];
      char digest[33];
      mutt_md5(md5_test_data[i].text, buf);
      mutt_md5_toascii(buf, digest);
      if (!TEST_CHECK(strcmp(md5_test_data[i].hash, digest) == 0))
      {
        TEST_MSG("Iteration: %zu", i);
        TEST_MSG("Expected : %s", md5_test_data[i].hash);
        TEST_MSG("Actual   : %s", digest);
      }
    }
  }
}
