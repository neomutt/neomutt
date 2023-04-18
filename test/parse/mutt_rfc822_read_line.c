/**
 * @file
 * Test code for mutt_rfc822_read_line()
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
#include "test_common.h"
#include <stdio.h>
#include "email/lib.h"

static struct Rfc822ReadLineTestData
{
  char *input;
  const char *output;
  size_t      read;
} test_data[] = {
  {
    "Subject: long\r\n  subject\r\n\r\n", 
    "Subject: long subject",
    26
  }
};

void test_mutt_rfc822_read_line(void)
{
  // size_t mutt_rfc822_read_line(FILE *fp, struct Buffer *buf);

  {
    struct Buffer buf = { 0 };
    TEST_CHECK(mutt_rfc822_read_line(NULL, &buf) == 0);
  }

  {
    FILE fp = { 0 };
    TEST_CHECK(mutt_rfc822_read_line(&fp, NULL) == 0);
  }

  for (size_t i = 0; i < mutt_array_size(test_data); ++i)
  {
    FILE *fp = fmemopen(test_data[i].input, strlen(test_data[i].input), "r");
    struct Buffer *buf = buf_pool_get();
    const size_t read = mutt_rfc822_read_line(fp, buf);
    long off = ftell(fp);
    if (!TEST_CHECK(read == test_data[i].read))
    {
      TEST_MSG("Expected: %zu", test_data[i].read);
      TEST_MSG("Actual  : %zu", read);
    }
    if (!TEST_CHECK(read == off))
    {
      TEST_MSG("Expected: %ld", off);
      TEST_MSG("Actual  : %zu", read);
    }
    TEST_CHECK_STR_EQ(test_data[i].output, buf_string(buf));
    buf_pool_release(&buf);
    fclose(fp);
  }
}
