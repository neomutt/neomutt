/**
 * @file
 * Test code for mutt_buffer_strdup()
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
#include <string.h>
#include "mutt/lib.h"

void test_mutt_buffer_strdup(void)
{
  // char *mutt_buffer_strdup(struct Buffer *buf);

  {
    char *str = mutt_buffer_strdup(NULL);
    TEST_CHECK(str == NULL);
  }

  {
    char *src = "abcdefghij";
    char *result = NULL;

    struct Buffer buf = mutt_buffer_make(32);

    mutt_buffer_strcpy(&buf, src);

    result = mutt_buffer_strdup(&buf);

    TEST_CHECK(result != NULL);
    TEST_CHECK(mutt_str_equal(result, src));

    FREE(&result);

    mutt_buffer_dealloc(&buf);
  }
}
