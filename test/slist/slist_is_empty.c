/**
 * @file
 * Test code for slist_is_empty()
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

void test_slist_is_empty(void)
{
  // bool slist_is_empty(const struct Slist *list);

  TEST_CHECK(slist_is_empty(NULL));

  struct Slist *slist = slist_new(SLIST_SEP_COMMA);
  TEST_CHECK(slist_is_empty(slist));
  slist_free(&slist);
}
