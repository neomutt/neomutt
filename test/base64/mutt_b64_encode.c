/**
 * @file
 * Test code for mutt_b64_encode()
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
#include <string.h>
#include "mutt/lib.h"

static const char clear[] = "Hello";
static const char encoded[] = "SGVsbG8=";

void test_mutt_b64_encode(void)
{
  // size_t mutt_b64_encode(const char *in, size_t inlen, char *out, size_t outlen);

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_b64_encode(NULL, 5, buf, sizeof(buf)) == 0);
  }

  {
    TEST_CHECK(mutt_b64_encode("apple", 5, NULL, 10) == 0);
  }

  {
    char buffer[16] = { 0 };
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

  {
    const char *in = "FuseMuse";
    char out1[32] = { 0 };
    size_t enclen;

    /* Encoding a zero-length string should fail */
    enclen = mutt_b64_encode(in, 0, out1, 32);
    if (!TEST_CHECK(enclen == 0))
    {
      TEST_MSG("Expected: %zu", 0);
      TEST_MSG("Actual  : %zu", enclen);
    }
  }

  {
    const char *in = "FuseMuse";
    char out1[32] = { 0 };
    char out2[32] = { 0 };

    /* Encode one to eight bytes, check the lengths of the returned string */
    for (size_t i = 1; i <= 8; i++)
    {
      size_t enclen = mutt_b64_encode(in, i, out1, 32);
      size_t exp = ((i + 2) / 3) << 2;
      if (!TEST_CHECK(enclen == exp))
      {
        TEST_MSG("Expected: %zu", exp);
        TEST_MSG("Actual  : %zu", enclen);
      }
      int declen = mutt_b64_decode(out1, out2, sizeof(out2));
      if (!TEST_CHECK(declen == i))
      {
        TEST_MSG("Expected: %zu", i);
        TEST_MSG("Actual  : %zu", declen);
      }
      out2[declen] = '\0';
      if (!TEST_CHECK(mutt_strn_equal(out2, in, i)))
      {
        TEST_MSG("Expected: %s", in);
        TEST_MSG("Actual  : %s", out2);
      }
    }
  }
}
