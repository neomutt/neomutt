/**
 * @file
 * Test code for mutt_addr_mbox_to_udomain()
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
#include "acutest.h"
#include "config.h"
#include "mutt/mutt.h"
#include "address/lib.h"
#include "common.h"

void test_mutt_addr_mbox_to_udomain(void)
{
  // int mutt_addr_mbox_to_udomain(const char *mbox, char **user, char **domain);

  {
    char *user = NULL;
    char *domain = NULL;
    TEST_CHECK(mutt_addr_mbox_to_udomain(NULL, &user, &domain) == -1);
  }

  {
    char *domain = NULL;
    TEST_CHECK(mutt_addr_mbox_to_udomain("apple", NULL, &domain) == -1);
  }

  {
    char *user = NULL;
    TEST_CHECK(mutt_addr_mbox_to_udomain("apple", &user, NULL) == -1);
  }

  { /* edge cases */
    char *user = NULL;
    char *domain = NULL;

    if (!TEST_CHECK(mutt_addr_mbox_to_udomain("bobnodomain@", &user, &domain) == -1) ||
        !TEST_CHECK(mutt_addr_mbox_to_udomain("bobnodomain", &user, &domain) == -1) ||
        !TEST_CHECK(mutt_addr_mbox_to_udomain("@nobobohnoez", &user, &domain) == -1) ||
        !TEST_CHECK(mutt_addr_mbox_to_udomain("", &user, &domain) == -1))
    {
      TEST_MSG("bad return");
    }
  }

  { /* common */
    int ret = 0;
    const char *str = "bob@bobsdomain";

    char *user = NULL;
    char *domain = NULL;
    ret = mutt_addr_mbox_to_udomain(str, &user, &domain);

    if (!TEST_CHECK(ret == 0))
    {
      TEST_MSG("bad return");
      TEST_MSG("Expected: %d", 0);
      TEST_MSG("Actual  : %d", ret);
    }

    TEST_CHECK_STR_EQ("bob", user);
    TEST_CHECK_STR_EQ("bobsdomain", domain);

    FREE(&user);
    FREE(&domain);
  }
}
