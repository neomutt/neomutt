/**
 * @file
 * Test code for buf_strdup()
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
#include "test_common.h"

void test_buf_strdup(void)
{
  // char *buf_strdup(struct Buffer *buf);

  {
    char *str = buf_strdup(NULL);
    TEST_CHECK(str == NULL);
  }

  {
    char *src = "abcdefghij";
    char *result = NULL;

    struct Buffer *buf = buf_pool_get();

    buf_strcpy(buf, src);

    result = buf_strdup(buf);

    TEST_CHECK(result != NULL);
    TEST_CHECK_STR_EQ(result, src);

    FREE(&result);

    buf_pool_release(&buf);
  }
}
