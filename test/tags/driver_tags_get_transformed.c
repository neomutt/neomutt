/**
 * @file
 * Test code for driver_tags_get_transformed()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2024 Dennis Schön <mail@dennis-schoen.de>
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
#include "email/lib.h"
#include "test_common.h"

void test_driver_tags_get_transformed(void)
{
  // char *driver_tags_get_transformed(struct TagList *list);

  struct Tags
  {
    struct Tag *tag;
    const char *str;
    char sep;
    const char *result;
  };

  {
    // clang-format off
    struct Tag tags[] = {
      { "foo",    "banana", false },
      { "bar",    "apple",  false },
      { "blubb",  "peach",  false },
      { "hidden", "hidden", true  }
    };
    // clang-format on

    struct TagList tl = STAILQ_HEAD_INITIALIZER(tl);

    for (size_t i = 0; i < mutt_array_size(tags); i++)
    {
      STAILQ_INSERT_TAIL(&tl, &tags[i], entries);
    }

    struct Buffer *buf = buf_pool_get();
    driver_tags_get_transformed(&tl, buf);
    TEST_CHECK_STR_EQ(buf_string(buf), "banana apple peach");
    buf_pool_release(&buf);
  }
}
