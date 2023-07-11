/**
 * @file
 * Test code for mutt_path_canon()
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
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "test_common.h"

void test_mutt_path_canon(void)
{
  // bool mutt_path_canon(struct Buffer *path, const char *homedir, bool is_dir);

  {
    TEST_CHECK(!mutt_path_canon(NULL, "apple", true));
  }

  // test already canonical

  {
    struct Buffer *path = buf_new("/apple");
    TEST_CHECK(mutt_path_canon(path, "/orange", true));
    TEST_CHECK_STR_EQ(path->data, "/apple");
    buf_free(&path);
  }

  // test homedir expansion

  {
    struct Buffer *path = buf_new("~/apple");
    TEST_CHECK(mutt_path_canon(path, "/orange", true));
    TEST_CHECK_STR_EQ(path->data, "/orange/apple");
    buf_free(&path);
  }

  // test current working directory expansion

  {
    struct Buffer *path = buf_new("./apple");
    char expected[PATH_MAX];
    snprintf(expected, sizeof(expected), "%s/apple", get_test_dir());

    TEST_ASSERT(chdir(get_test_dir()) == 0);
    TEST_CHECK(mutt_path_canon(path, "", true));
    TEST_CHECK_STR_EQ(buf_string(path), expected);

    buf_free(&path);
  }
}
