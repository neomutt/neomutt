/**
 * @file
 * Test code for mutt_path_realpath()
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
#include "mutt/lib.h"
#include "test_common.h"

void test_mutt_path_realpath(void)
{
  // size_t mutt_path_realpath(struct Buffer *path);

  {
    TEST_CHECK(mutt_path_realpath(NULL) == 0);
  }

  {
    // Working symlink
    struct Buffer *path = buf_pool_get();
    char expected[PATH_MAX] = { 0 };

    const char *test_dir = get_test_dir();
    TEST_CHECK(test_dir != NULL);
    buf_printf(path, "%s/file/empty_symlink", test_dir);
    snprintf(expected, sizeof(expected), "%s/file/empty", test_dir);

    TEST_CHECK(mutt_path_realpath(path) > 0);
    TEST_CHECK_STR_EQ(buf_string(path), expected);
    buf_pool_release(&path);
  }

  {
    // Broken symlink
    struct Buffer *path = buf_pool_get();
    char expected[PATH_MAX] = { 0 };

    const char *test_dir = get_test_dir();
    TEST_CHECK(test_dir != NULL);
    buf_printf(path, "%s/file/missing_symlink", test_dir);
    snprintf(expected, sizeof(expected), "%s/file/missing_symlink", test_dir);

    TEST_CHECK(mutt_path_realpath(path) == 0);
    TEST_CHECK_STR_EQ(buf_string(path), expected);
    buf_pool_release(&path);
  }
}
