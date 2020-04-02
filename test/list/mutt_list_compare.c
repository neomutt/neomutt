/**
 * @file
 * Test code for mutt_list_compare()
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

void test_mutt_list_compare(void)
{
  // bool mutt_list_compare(const struct ListHead *ah, const struct ListHead *bh);

  {
    struct ListHead listhead = { 0 };
    TEST_CHECK(!mutt_list_compare(NULL, &listhead));
  }

  {
    struct ListHead listhead = { 0 };
    TEST_CHECK(!mutt_list_compare(&listhead, NULL));
  }

  {
    struct ListHead first = STAILQ_HEAD_INITIALIZER(first);
    struct ListHead second = STAILQ_HEAD_INITIALIZER(second);
    TEST_CHECK(mutt_list_compare(&first, &second) == true);
  }

  {
    static const char *names[] = { "Amy", "Beth", "Cathy", NULL };
    struct ListHead first = test_list_create(names, false);
    struct ListHead second = STAILQ_HEAD_INITIALIZER(second);
    TEST_CHECK(mutt_list_compare(&first, &second) == false);
    mutt_list_clear(&first);
  }

  {
    static const char *first_names[] = { "Amy", "Beth", "Cathy", NULL };
    static const char *second_names[] = { "Amy", "Beth", NULL };
    struct ListHead first = test_list_create(first_names, false);
    struct ListHead second = test_list_create(second_names, false);
    TEST_CHECK(mutt_list_compare(&first, &second) == false);
    mutt_list_clear(&first);
    mutt_list_clear(&second);
  }

  {
    static const char *first_names[] = { "Amy", "Beth", "Cathy", NULL };
    static const char *second_names[] = { "Amy", "Beth", "Cathy", NULL };
    struct ListHead first = test_list_create(first_names, false);
    struct ListHead second = test_list_create(second_names, false);
    TEST_CHECK(mutt_list_compare(&first, &second) == true);
    mutt_list_clear(&first);
    mutt_list_clear(&second);
  }

  {
    static const char *first_names[] = { "Amy", "Beth", "Cathy", NULL };
    static const char *second_names[] = { "Amy", "Beth", "Cathy", "Denise", NULL };
    struct ListHead first = test_list_create(first_names, false);
    struct ListHead second = test_list_create(second_names, false);
    TEST_CHECK(mutt_list_compare(&first, &second) == false);
    mutt_list_clear(&first);
    mutt_list_clear(&second);
  }

  {
    static const char *first_names[] = { "Amy", "Beth", "Cathy", NULL };
    static const char *second_names[] = { "Anna", "Beth", "Cathy", NULL };
    struct ListHead first = test_list_create(first_names, false);
    struct ListHead second = test_list_create(second_names, false);
    TEST_CHECK(mutt_list_compare(&first, &second) == false);
    mutt_list_clear(&first);
    mutt_list_clear(&second);
  }

  {
    static const char *first_names[] = { "Amy", "Beth", "Cathy", NULL };
    static const char *second_names[] = { "Amy", "Bella", "Cathy", NULL };
    struct ListHead first = test_list_create(first_names, false);
    struct ListHead second = test_list_create(second_names, false);
    TEST_CHECK(mutt_list_compare(&first, &second) == false);
    mutt_list_clear(&first);
    mutt_list_clear(&second);
  }

  {
    static const char *first_names[] = { "Amy", "Beth", "Cathy", NULL };
    static const char *second_names[] = { "Anna", "Beth", "Carol", NULL };
    struct ListHead first = test_list_create(first_names, false);
    struct ListHead second = test_list_create(second_names, false);
    TEST_CHECK(mutt_list_compare(&first, &second) == false);
    mutt_list_clear(&first);
    mutt_list_clear(&second);
  }
}
