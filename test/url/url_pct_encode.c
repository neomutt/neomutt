/**
 * @file
 * Test code for url_pct_encode()
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
    if (!TEST_CHECK(strcmp(buf, "Hello%20world") == 0))
    {
      TEST_MSG("Expected: %s", "Hello%20world");
      TEST_MSG("Actual  : %s", buf);
    }
  }
}
