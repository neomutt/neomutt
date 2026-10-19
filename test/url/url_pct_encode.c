/**
 * @file
 * Test code for url_pct_encode()
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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
#include "email/lib.h"
#include "test_common.h"

void test_url_pct_encode(void)
{
  // void url_pct_encode(char *buf, size_t buflen, const char *src);

  {
    url_pct_encode(NULL, 10, "apple");
    TEST_CHECK_(1, "url_pct_encode(NULL, 10, \"apple\")");
  }

  {
    char buf[32] = { 0 };
    url_pct_encode(buf, sizeof(buf), NULL);
    TEST_CHECK_(1, "url_pct_encode(&buf, sizeof(buf), NULL)");
  }

  {
    char buf[32] = { 0 };
    url_pct_encode(buf, sizeof(buf), "Hello world");
    TEST_CHECK_STR_EQ(buf, "Hello%20world");
  }

  {
    // Characters a URL can't carry literally
    char buf[64] = { 0 };
    url_pct_encode(buf, sizeof(buf), "domain\\user");
    TEST_CHECK_STR_EQ(buf, "domain%5Cuser");

    url_pct_encode(buf, sizeof(buf), "a\"b#c<d>e?f^g`h{i|j}k");
    TEST_CHECK_STR_EQ(buf, "a%22b%23c%3Cd%3Ee%3Ff%5Eg%60h%7Bi%7Cj%7Dk");

    url_pct_encode(buf, sizeof(buf), "tab\there");
    TEST_CHECK_STR_EQ(buf, "tab%09here");
  }

  {
    // Characters a URL can carry literally are left alone
    char buf[64] = { 0 };
    url_pct_encode(buf, sizeof(buf), "user@example.com-_.~!$'()*+,;");
    TEST_CHECK_STR_EQ(buf, "user@example.com-_.~!$'()*+,;");
  }
}
