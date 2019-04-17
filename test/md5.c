/**
 * @file
 * Test code for MD5 Encoding
 *
 * @authors
 * Copyright (C) 2018 Pietro Cerutti <gahr@gahr.ch>
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

#include "mutt/md5.h"
#include "mutt/memory.h"

static const struct
{
  const char *text; /* clear text input string */
  const char *hash; /* MD5 hash digest         */
} test_data[] =
    /* clang-format off */
{
  {
    "The quick brown fox jumps over the lazy dog",
    "9e107d9d372bb6826bd81d3542a419d6"
  },
  {
    "", // The empty string
    "d41d8cd98f00b204e9800998ecf8427e"
  }
};
/* clang-format on */

void test_md5(void)
{
  for (size_t i = 0; i < mutt_array_size(test_data); ++i)
  {
    unsigned char buf[16];
    char digest[33];
    mutt_md5(test_data[i].text, buf);
    mutt_md5_toascii(buf, digest);
    if (!TEST_CHECK(strcmp(test_data[i].hash, digest) == 0))
    {
      TEST_MSG("Iteration: %zu", i);
      TEST_MSG("Expected : %s", test_data[i].hash);
      TEST_MSG("Actual   : %s", digest);
    }
  }
}

void test_md5_ctx(void)
{
  for (size_t i = 0; i < mutt_array_size(test_data); ++i)
  {
    struct Md5Ctx ctx;
    unsigned char buf[16];
    char digest[33];
    mutt_md5_init_ctx(&ctx);
    mutt_md5_process(test_data[i].text, &ctx);
    mutt_md5_finish_ctx(&ctx, buf);
    mutt_md5_toascii(buf, digest);
    if (!TEST_CHECK(strcmp(test_data[i].hash, digest) == 0))
    {
      TEST_MSG("Iteration: %zu", i);
      TEST_MSG("Expected : %s", test_data[i].hash);
      TEST_MSG("Actual   : %s", digest);
    }
  }
}

void test_md5_ctx_bytes(void)
{
  for (size_t i = 0; i < mutt_array_size(test_data); ++i)
  {
    struct Md5Ctx ctx;
    unsigned char buf[16];
    char digest[33];
    mutt_md5_init_ctx(&ctx);
    mutt_md5_process_bytes(test_data[i].text, strlen(test_data[i].text), &ctx);
    mutt_md5_finish_ctx(&ctx, buf);
    mutt_md5_toascii(buf, digest);
    if (!TEST_CHECK(strcmp(test_data[i].hash, digest) == 0))
    {
      TEST_MSG("Iteration: %zu", i);
      TEST_MSG("Expected : %s", test_data[i].hash);
      TEST_MSG("Actual   : %s", digest);
    }
  }
}

