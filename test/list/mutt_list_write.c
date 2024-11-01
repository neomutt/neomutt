/**
 * @file
 * Test code for mutt_list_write()
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
#include <string.h>
#include "mutt/lib.h"
#include "common.h"
#include "test_common.h"

void test_mutt_list_write(void)
{
  // void mutt_list_write(const struct ListHead *h, struct Buffer *buf);

  {
    struct ListHead empty;
    STAILQ_INIT(&empty);
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK(mutt_list_write(NULL, buf) == 0);
    TEST_CHECK(mutt_list_write(&empty, NULL) == 0);
    buf_pool_release(&buf);
  }

  {
    struct ListHead empty;
    STAILQ_INIT(&empty);
    struct Buffer *buf = buf_pool_get();
    const char expected[] = "";
    TEST_CHECK(mutt_list_write(&empty, buf) == strlen(expected));
    TEST_CHECK_STR_EQ(buf_string(buf), expected);
    buf_pool_release(&buf);
  }

  {
    static const char *list_names[] = { "Amy", "Beth", "Cathy", NULL };
    struct ListHead list = test_list_create(list_names, false);
    struct Buffer *buf = buf_pool_get();
    const char expected[] = "Amy Beth Cathy";
    TEST_CHECK(mutt_list_write(&list, buf) == strlen(expected));
    TEST_CHECK_STR_EQ(buf_string(buf), expected);
    buf_pool_release(&buf);
    mutt_list_clear(&list);
  }
}
