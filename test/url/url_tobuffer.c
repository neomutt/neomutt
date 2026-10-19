/**
 * @file
 * Test code for url_tobuffer()
 *
 * @authors
 * Copyright (C) 2019-2021 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
#include <stddef.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "test_common.h"

void test_url_tobuffer(void)
{
  // int url_tobuffer(struct Url *url, struct Buffer *buf, uint8_t flags);

  {
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK(url_tobuffer(NULL, buf, U_NONE) != 0);
    buf_pool_release(&buf);
  }

  {
    struct Url url = { 0 };
    TEST_CHECK(url_tobuffer(&url, NULL, U_NONE) != 0);
  }

  {
    // A user name with characters a URL can't carry literally survives a
    // round trip through url_tobuffer() and url_parse()
    static const char *users[] = { "domain\\user", "user#mailbox", "who?", "a\"b" };
    for (size_t i = 0; i < countof(users); i++)
    {
      TEST_CASE(users[i]);
      struct Url url = { 0 };
      url.scheme = U_IMAPS;
      url.user = (char *) users[i];
      url.host = "example.com";
      url.path = "INBOX";

      struct Buffer *buf = buf_pool_get();
      TEST_CHECK(url_tobuffer(&url, buf, U_NONE) == 0);

      struct Url *parsed = url_parse(buf_string(buf));
      if (TEST_CHECK(parsed != NULL))
      {
        TEST_CHECK_STR_EQ(parsed->user, users[i]);
        TEST_CHECK_STR_EQ(parsed->host, "example.com");
        TEST_CHECK_STR_EQ(parsed->path, "INBOX");
      }
      else
      {
        TEST_MSG("Failed to parse: %s", buf_string(buf));
      }

      url_free(&parsed);
      buf_pool_release(&buf);
    }
  }
}
