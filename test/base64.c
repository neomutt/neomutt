/**
 * @file
 * Test code for Base 64 Encoding
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

#include "mutt/base64.h"

#include <string.h>

static const char clear[] = "Hello";
static const char encoded[] = "SGVsbG8=";

void test_base64_encode(void)
{
  char buffer[16];
  size_t len = mutt_b64_encode(clear, sizeof(clear) - 1, buffer, sizeof(buffer));
  if (!TEST_CHECK(len == sizeof(encoded) - 1))
  {
    TEST_MSG("Expected: %zu", sizeof(encoded) - 1);
    TEST_MSG("Actual  : %zu", len);
  }
  if (!TEST_CHECK(strcmp(buffer, encoded) == 0))
  {
    TEST_MSG("Expected: %zu", encoded);
    TEST_MSG("Actual  : %zu", buffer);
  }
}

void test_base64_decode(void)
{
  char buffer[16];
  int len = mutt_b64_decode(encoded, buffer, sizeof(buffer));
  if (!TEST_CHECK(len == sizeof(clear) - 1))
  {
    TEST_MSG("Expected: %zu", sizeof(clear) - 1);
    TEST_MSG("Actual  : %zu", len);
  }
  buffer[len] = '\0';
  if (!TEST_CHECK(strcmp(buffer, clear) == 0))
  {
    TEST_MSG("Expected: %s", clear);
    TEST_MSG("Actual  : %s", buffer);
  }
}

void test_base64_lengths(void)
{
  const char *in = "FuseMuse";
  char out1[32];
  char out2[32];
  size_t enclen;
  int declen;

  /* Encoding a zero-length string should fail */
  enclen = mutt_b64_encode(in, 0, out1, 32);
  if (!TEST_CHECK(enclen == 0))
  {
    TEST_MSG("Expected: %zu", 0);
    TEST_MSG("Actual  : %zu", enclen);
  }

  /* Decoding a zero-length string should fail, too */
  out1[0] = '\0';
  declen = mutt_b64_decode(out1, out2, sizeof(out2));
  if (!TEST_CHECK(declen == -1))
  {
    TEST_MSG("Expected: %zu", -1);
    TEST_MSG("Actual  : %zu", declen);
  }

  /* Encode one to eight bytes, check the lengths of the returned string */
  for (size_t i = 1; i <= 8; ++i)
  {
    enclen = mutt_b64_encode(in, i, out1, 32);
    size_t exp = ((i + 2) / 3) << 2;
    if (!TEST_CHECK(enclen == exp))
    {
      TEST_MSG("Expected: %zu", exp);
      TEST_MSG("Actual  : %zu", enclen);
    }
    declen = mutt_b64_decode(out1, out2, sizeof(out2));
    if (!TEST_CHECK(declen == i))
    {
      TEST_MSG("Expected: %zu", i);
      TEST_MSG("Actual  : %zu", declen);
    }
    out2[declen] = '\0';
    if (!TEST_CHECK(strncmp(out2, in, i) == 0))
    {
      TEST_MSG("Expected: %s", in);
      TEST_MSG("Actual  : %s", out2);
    }
  }
}
