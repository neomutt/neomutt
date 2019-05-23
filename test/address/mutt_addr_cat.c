/**
 * @file
 * Test code for mutt_addr_cat()
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
#include "acutest.h"
#include "config.h"
#include "mutt/mutt.h"
#include "address/lib.h"
#include "common.h"

void test_mutt_addr_cat(void)
{
  // void mutt_addr_cat(char *buf, size_t buflen, const char *value, const char *specials);

  const char MimeSpecials[] = "@.,;:<>[]\\\"()?/= \t";

  {
    mutt_addr_cat(NULL, 32, "apple", MimeSpecials);
    TEST_CHECK_(1, "mutt_addr_cat(NULL, 32, \"apple\", MimeSpecials)");
  }

  {
    char buf[32];
    mutt_addr_cat(buf, sizeof(buf), NULL, MimeSpecials);
    TEST_CHECK_(1, "mutt_addr_cat(buf, sizeof(buf), NULL, MimeSpecials)");
  }

  {
    char buf[32];
    mutt_addr_cat(buf, sizeof(buf), "apple", NULL);
    TEST_CHECK_(1, "mutt_addr_cat(buf, sizeof(buf), \"apple\", NULL)");
  }

  {
    char buf[32];
    mutt_addr_cat(buf, 0, "apple", MimeSpecials);
    TEST_CHECK_(1, "mutt_addr_cat(buf, 0, \"apple\", MimeSpecials)");
  }

  {
    char buf[32];
    mutt_addr_cat(buf, sizeof(buf), "apple", MimeSpecials);
    TEST_CHECK_STR_EQ("apple", buf);
  }

  {
    char buf[32];
    mutt_addr_cat(buf, sizeof(buf), "a(pp)le", MimeSpecials);
    TEST_CHECK_STR_EQ("\"a(pp)le\"", buf);
  }

  {
    char buf[32];
    mutt_addr_cat(buf, sizeof(buf), "a(pp)l\"e", MimeSpecials);
    TEST_CHECK_STR_EQ("\"a(pp)l\\\"e\"", buf);
  }
}
