/**
 * @file
 * Test code for mutt_list_copy_tail()
 *
 * @authors
 * Copyright (C) 2024 Alejandro Colomar <alx@kernel.org>
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
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "common.h"

void test_mutt_list_copy_tail(void)
{
  // void mutt_list_copy_tail(struct ListHead *dst, const struct ListHead *src);

  {
    static const char *expected_names[] = { "Zelda", "Mario", NULL };
    static const char *copy_names[] = { "Zelda", "Mario", NULL };
    struct ListHead start;
    struct ListHead expected = test_list_create(expected_names, true);
    struct ListHead copy = test_list_create(copy_names, true);
    STAILQ_INIT(&start);
    mutt_list_copy_tail(&start, &copy);
    TEST_CHECK(mutt_list_equal(&start, &expected) == true);
    mutt_list_free(&start);
    mutt_list_free(&expected);
    mutt_list_free(&copy);
  }

  {
    static const char *start_names[] = { "Amy", "Beth", "Cathy", NULL };
    static const char *expected_names[] = { "Amy",   "Beth",  "Cathy",
                                            "Zelda", "Mario", NULL };
    static const char *copy_names[] = { "Zelda", "Mario", NULL };
    struct ListHead start = test_list_create(start_names, true);
    struct ListHead expected = test_list_create(expected_names, true);
    struct ListHead copy = test_list_create(copy_names, true);
    mutt_list_copy_tail(&start, &copy);
    TEST_CHECK(mutt_list_equal(&start, &expected) == true);
    mutt_list_free(&start);
    mutt_list_free(&expected);
    mutt_list_free(&copy);
  }
}
