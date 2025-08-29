/**
 * @file
 * Test code for rfc2047_encode()
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 наб <nabijaczleweli@nabijaczleweli.xyz>
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
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "common.h"
#include "test_common.h"

void test_rfc2047_encode(void)
{
  // void rfc2047_encode(char **pd, const char *specials, int col, const struct Slist *charsets);

  {
    struct Slist *charsets = slist_parse("apple", D_SLIST_SEP_COLON);
    rfc2047_encode(NULL, AddressSpecials, 0, charsets);
    TEST_CHECK_(1, "rfc2047_encode(NULL, AddressSpecials, 0, \"apple\")");
    slist_free(&charsets);
  }

  {
    char *pd = NULL;
    struct Slist *charsets = slist_parse("apple", D_SLIST_SEP_COLON);
    rfc2047_encode(&pd, NULL, 0, charsets);
    TEST_CHECK_(1, "rfc2047_encode(&pd, NULL, 0, \"apple\")");
    slist_free(&charsets);
  }

  {
    char *pd = NULL;
    rfc2047_encode(&pd, AddressSpecials, 0, NULL);
    TEST_CHECK_(1, "rfc2047_encode(&pd, AddressSpecials, 0, NULL)");
  }

  {
    struct Slist *charsets = slist_parse("utf-8", D_SLIST_SEP_COLON);
    for (size_t i = 0; rfc2047_test_data[i].decoded; i++)
    {
      /* encode the expected result */
      char *s = mutt_str_dup(rfc2047_test_data[i].decoded);
      rfc2047_encode(&s, NULL, 0, charsets);
      TEST_CHECK_STR_EQ(s, rfc2047_test_data[i].encoded);
      FREE(&s);
    }
    slist_free(&charsets);
  }
}
