/**
 * @file
 * Test code for mutt_addrlist_write()
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019-2023 Pietro Cerutti <gahr@gahr.ch>
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
#include <stdbool.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "test_common.h"

static const char *test_name(const char *str)
{
  if (!str)
    return "[NULL]";
  if (!*str)
    return "[empty]";
  return str;
}

struct TestCase
{
  const char *address_list;
  const char *header;
  bool display;
  int cols;
  size_t ret_num;
  const char *expected;
};

// size_t mutt_addrlist_write(const struct AddressList *al, struct Buffer *buf,
//                             bool display, const char *header, int cols)

void test_mutt_addrlist_write(void)
{
  static const struct TestCase tests[] = {
    { NULL, NULL, false, -1, 0, "" },
    { "undisclosed-recipients:;", NULL, false, -1, 25, "undisclosed-recipients: ;" },
    {
        "test@example.com, John Doe <john@doe.org>, \"Foo J. Bar\" <foo-j-bar@baz.com>",
        NULL,
        false,
        -1,
        75,
        "test@example.com, John Doe <john@doe.org>, \"Foo J. Bar\" <foo-j-bar@baz.com>",
    },
    {
        "some-group: first@example.com, second@example.com;, John Doe <john@doe.org>, \"Foo J. Bar\" <foo-j-bar@baz.com>",
        NULL,
        false,
        -1,
        109,
        "some-group: first@example.com, second@example.com;, John Doe <john@doe.org>, \"Foo J. Bar\" <foo-j-bar@baz.com>",
    },
    {
        "foo@bar.com, sooooooooooooooooooooooooomthing@looooooooooooooooooooooooong.com, foo@bar.com",
        "To",
        false,
        -1,
        95,
        "To: foo@bar.com, sooooooooooooooooooooooooomthing@looooooooooooooooooooooooong.com, foo@bar.com",
    },
    {
        "foo@bar.com, sooooooooooooooooooooooooomthing@looooooooooooooooooooooooong.com, foo@bar.com",
        "To",
        false,
        74,
        97,
        "To: foo@bar.com, \n\tsooooooooooooooooooooooooomthing@looooooooooooooooooooooooong.com, foo@bar.com",
    },
    {
        "foo@bar.com,foo@bar.com,foo@bar.com,foo@bar.com,foo@bar.com,foo@bar.com,foo@bar.com",
        "To",
        false,
        -1,
        93,
        "To: foo@bar.com, foo@bar.com, foo@bar.com, foo@bar.com, foo@bar.com, foo@bar.com, foo@bar.com",
    },
    {
        "foo@bar.com,foo@bar.com,foo@bar.com,foo@bar.com,foo@bar.com,foo@bar.com,foo@bar.com",
        "To",
        false,
        74,
        95,
        "To: foo@bar.com, foo@bar.com, foo@bar.com, foo@bar.com, foo@bar.com, \n\tfoo@bar.com, foo@bar.com",
    },
  };

  {
    for (size_t i = 0; i < mutt_array_size(tests); i++)
    {
      TEST_CASE(test_name(tests[i].address_list));

      struct AddressList al = { 0 };
      if (tests[i].address_list)
      {
        struct AddressList parsed = TAILQ_HEAD_INITIALIZER(parsed);
        mutt_addrlist_parse(&parsed, tests[i].address_list);
        al = parsed;
      }

      struct Buffer *buf = buf_pool_get();
      int buf_len = mutt_addrlist_write(&al, buf, tests[i].display,
                                        tests[i].header, tests[i].cols);
      TEST_CHECK(buf_len == tests[i].ret_num);
      TEST_CHECK_STR_EQ(buf_string(buf), tests[i].expected);
      buf_pool_release(&buf);
      mutt_addrlist_clear(&al);
    }
  }

  { // no buffer
    struct AddressList al = { 0 };
    TEST_CHECK(mutt_addrlist_write(&al, NULL, false, NULL, -1) == 0);
  }
}
