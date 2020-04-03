/**
 * @file
 * Test code for mutt_regex_new()
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

void test_mutt_regex_new(void)
{
  // struct Regex *mutt_regex_new(const char *str, int flags, struct Buffer *err);

  {
    struct Buffer buf = mutt_buffer_make(0);
    TEST_CHECK(!mutt_regex_new(NULL, 0, &buf));
  }

  {
    struct Regex *regex = NULL;
    TEST_CHECK((regex = mutt_regex_new("apple", 0, NULL)) != NULL);
    mutt_regex_free(&regex);
  }
}
