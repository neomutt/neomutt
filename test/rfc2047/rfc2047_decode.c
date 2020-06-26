/**
 * @file
 * Test code for rfc2047_decode()
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
#include "address/lib.h"
#include "email/lib.h"
#include "common.h"

void test_rfc2047_decode(void)
{
  // void rfc2047_decode(char **pd);

  char *previous_charset = C_Charset;
  C_Charset = "utf-8";

  {
    rfc2047_decode(NULL);
    TEST_CHECK_(1, "rfc2047_decode(NULL)");
  }

  {
    char *pd = NULL;
    rfc2047_decode(&pd);
    TEST_CHECK_(1, "rfc2047_decode(&pd)");
  }

  {
    for (size_t i = 0; rfc2047_test_data[i].original; i++)
    {
      /* decode the original string */
      char *s = mutt_str_dup(rfc2047_test_data[i].original);
      rfc2047_decode(&s);
      if (!TEST_CHECK(strcmp(s, rfc2047_test_data[i].decoded) == 0))
      {
        TEST_MSG("Iteration: %zu", i);
        TEST_MSG("Expected : %s", rfc2047_test_data[i].decoded);
        TEST_MSG("Actual   : %s", s);
      }
      FREE(&s);

      /* decode the encoded result */
      s = mutt_str_dup(rfc2047_test_data[i].encoded);
      rfc2047_decode(&s);
      if (!TEST_CHECK(strcmp(s, rfc2047_test_data[i].decoded) == 0))
      {
        TEST_MSG("Iteration: %zu", i);
        TEST_MSG("Expected : %s", rfc2047_test_data[i].decoded);
        TEST_MSG("Actual   : %s", s);
      }
      FREE(&s);
    }
  }

  C_Charset = previous_charset;
}
