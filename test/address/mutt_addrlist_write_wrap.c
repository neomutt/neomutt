/**
 * @file
 * Test code for mutt_addrlist_write_wrap()
 *
 * @authors
 * Copyright (C) 2024 Dennis Schön <mail@dennis-schoen.de>
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
  size_t ret_num;
  const char *expected;
};

// size_t mutt_addrlist_write_wrap(const struct AddressList *al, struct Buffer *buf, const char *header)
void test_mutt_addrlist_write_wrap(void)
{
  static const struct TestCase tests[] = {
    { NULL, NULL, 0, "" },
    { "", "", 0, "" },
    {
        "foo@bar.com, sooooooooooooooooooooooooomthing@looooooooooooooooooooooooong.com, foo@bar.com",
        "To",
        97,
        "To: foo@bar.com, \n\tsooooooooooooooooooooooooomthing@looooooooooooooooooooooooong.com, foo@bar.com",
    },
    {
        "foo@bar.com,foo@bar.com,foo@bar.com,foo@bar.com,foo@bar.com,foo@bar.com,foo@bar.com",
        "To",
        95,
        "To: foo@bar.com, foo@bar.com, foo@bar.com, foo@bar.com, foo@bar.com, \n\tfoo@bar.com, foo@bar.com",
    },
  };

  {
    for (size_t i = 0; i < countof(tests); i++)
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
      int buf_len = mutt_addrlist_write_wrap(&al, buf, tests[i].header);
      TEST_CHECK(buf_len == tests[i].ret_num);
      TEST_CHECK_STR_EQ(buf_string(buf), tests[i].expected);
      buf_pool_release(&buf);
      mutt_addrlist_clear(&al);
    }
  }
}
