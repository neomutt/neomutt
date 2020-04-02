/**
 * @file
 * Test code for mutt_addrlist_write()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
#include "common.h"

void test_mutt_addrlist_write(void)
{
  // size_t mutt_addrlist_write(char *buf, size_t buflen, const struct AddressList *al, bool display);

  {
    struct AddressList al = { 0 };
    TEST_CHECK(mutt_addrlist_write(&al, NULL, 32, false) == 0);
  }

  {
    char buf[32] = { 0 };
    TEST_CHECK(mutt_addrlist_write(NULL, buf, sizeof(buf), false) == 0);
  }

  {
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    const char in[] = "test@example.com, John Doe <john@doe.org>, \"Foo J. "
                      "Bar\" <foo-j-bar@baz.com>";
    int parsed = mutt_addrlist_parse(&al, in);
    TEST_CHECK(parsed == 3);

    {
      char buf[8] = { 0 };
      size_t nbytes = mutt_addrlist_write(&al, buf, sizeof(buf), false);
      TEST_CHECK(nbytes == sizeof(buf) - 1);
      TEST_CHECK_STR_EQ("test@ex", buf);
    }

    {
      char buf[24] = { 0 };
      size_t nbytes = mutt_addrlist_write(&al, buf, sizeof(buf), false);
      TEST_CHECK(nbytes == sizeof(buf) - 1);
      TEST_CHECK_STR_EQ("test@example.com, John ", buf);
    }

    {
      char buf[43] = { 0 };
      size_t nbytes = mutt_addrlist_write(&al, buf, sizeof(buf), false);
      TEST_CHECK(nbytes == sizeof(buf) - 1);
      TEST_CHECK_STR_EQ("test@example.com, John Doe <john@doe.org>,", buf);
    }

    {
      char buf[76] = { 0 };
      size_t nbytes = mutt_addrlist_write(&al, buf, sizeof(buf), false);
      TEST_CHECK(nbytes == sizeof(buf) - 1);
      TEST_CHECK_STR_EQ("test@example.com, John Doe <john@doe.org>, \"Foo J. "
                        "Bar\" <foo-j-bar@baz.com>",
                        buf);
      TEST_CHECK(buf[sizeof(in) - 1] == '\0');
    }

    mutt_addrlist_clear(&al);
  }
}
