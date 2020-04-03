/**
 * @file
 * Test code for mutt_list_match()
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
#include "common.h"

void test_mutt_list_match(void)
{
  // bool mutt_list_match(const char *s, struct ListHead *h);

  {
    struct ListHead listhead = { 0 };
    TEST_CHECK(!mutt_list_match(NULL, &listhead));
  }

  {
    TEST_CHECK(!mutt_list_match("apple", NULL));
  }

  {
    static const char *names[] = { "Amy", "Beth", "Cathy", NULL };
    struct ListHead lh = test_list_create(names, false);
    const char *search = "AMY";
    TEST_CHECK(mutt_list_match(search, &lh) == true);
    mutt_list_clear(&lh);
  }

  {
    static const char *names[] = { "Amy", "Beth", "Cathy", NULL };
    struct ListHead lh = test_list_create(names, false);
    const char *search = "CAthy";
    TEST_CHECK(mutt_list_match(search, &lh) == true);
    mutt_list_clear(&lh);
  }

  {
    static const char *names[] = { "Amy", "Beth", "Cathy", NULL };
    struct ListHead lh = test_list_create(names, false);
    const char *search = "Bethany";
    TEST_CHECK(mutt_list_match(search, &lh) == true);
    mutt_list_clear(&lh);
  }

  {
    static const char *names[] = { "Amy", "Beth", "Cathy", NULL };
    struct ListHead lh = test_list_create(names, false);
    const char *search = "Cath";
    TEST_CHECK(mutt_list_match(search, &lh) == false);
    mutt_list_clear(&lh);
  }

  {
    static const char *names[] = { "Amy", "Beth", "Cathy", NULL };
    struct ListHead lh = test_list_create(names, false);
    const char *search = "Denise";
    TEST_CHECK(mutt_list_match(search, &lh) == false);
    mutt_list_clear(&lh);
  }

  {
    static const char *names[] = { "Amy", "Beth", "*", NULL };
    struct ListHead lh = test_list_create(names, false);
    const char *search = "Anything";
    TEST_CHECK(mutt_list_match(search, &lh) == true);
    mutt_list_clear(&lh);
  }
}
