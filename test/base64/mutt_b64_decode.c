/**
 * @file
 * Test code for mutt_b64_decode()
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
#include "test_common.h"

static const char clear[] = "Hello";
static const char encoded[] = "SGVsbG8=";

void test_mutt_b64_decode(void)
{
  // int mutt_b64_decode(const char *in, char *out, size_t olen);

  {
    TEST_CHECK(mutt_b64_decode(NULL, "banana", 10) != 0);
  }

  {
    TEST_CHECK(mutt_b64_decode("apple", NULL, 10) != 0);
  }

  {
    char buffer[16] = { 0 };
    int len = mutt_b64_decode(encoded, buffer, sizeof(buffer));
    if (!TEST_CHECK(len == sizeof(clear) - 1))
    {
      TEST_MSG("Expected: %zu", sizeof(clear) - 1);
      TEST_MSG("Actual  : %zu", len);
    }
    buffer[len] = '\0';
    TEST_CHECK_STR_EQ(buffer, clear);
  }

  {
    char in[32] = { 0 };
    char out[32] = { 0 };

    /* Decoding a zero-length string should fail, too */
    int declen = mutt_b64_decode(in, out, sizeof(out));
    if (!TEST_CHECK(declen == -1))
    {
      TEST_MSG("Expected: %zu", -1);
      TEST_MSG("Actual  : %zu", declen);
    }
  }

  {
    char in[32] = "JQ";
    char out[32] = { 0 };

    /* Decoding a non-padded string should be ok */
    int declen = mutt_b64_decode(in, out, sizeof(out));
    if (!TEST_CHECK(declen == 1))
    {
      TEST_MSG("Expected: %zu", 1);
      TEST_MSG("Actual  : %zu", declen);
    }
    TEST_CHECK_STR_EQ(out, "%");
  }

  {
    char in1[32] = "#A";
    char in2[32] = "A#";
    char in3[32] = "AA#A";
    char in4[32] = "AAA#";
    char out[32] = { 0 };

    int declen;

    declen = mutt_b64_decode(in1, out, sizeof(out));
    TEST_CHECK(declen == -1);

    declen = mutt_b64_decode(in2, out, sizeof(out));
    TEST_CHECK(declen == -1);

    declen = mutt_b64_decode(in3, out, sizeof(out));
    TEST_CHECK(declen == -1);

    declen = mutt_b64_decode(in4, out, sizeof(out));
    TEST_CHECK(declen == -1);
  }

  {
    char in[32] = "AAAA";
    char out[32] = { 0 };

    int declen;

    declen = mutt_b64_decode(in, out, 0);
    TEST_CHECK(declen == 0);

    declen = mutt_b64_decode(in, out, 1);
    TEST_CHECK(declen == 1);

    declen = mutt_b64_decode(in, out, 2);
    TEST_CHECK(declen == 2);
  }
}
