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
    if (!TEST_CHECK(strcmp(buffer, clear) == 0))
    {
      TEST_MSG("Expected: %s", clear);
      TEST_MSG("Actual  : %s", buffer);
    }
  }

  {
    char out1[32] = { 0 };
    char out2[32] = { 0 };

    /* Decoding a zero-length string should fail, too */
    int declen = mutt_b64_decode(out1, out2, sizeof(out2));
    if (!TEST_CHECK(declen == -1))
    {
      TEST_MSG("Expected: %zu", -1);
      TEST_MSG("Actual  : %zu", declen);
    }
  }
}
