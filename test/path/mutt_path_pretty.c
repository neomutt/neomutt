/**
 * @file
 * Test code for mutt_path_pretty()
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

void test_mutt_path_pretty(void)
{
  // bool mutt_path_pretty(struct Buffer *path, const char *homedir);

  {
    TEST_CHECK(!mutt_path_pretty(NULL, "/apple", true));
  }

  // test replacement

  {
    struct Buffer *path = buf_new("/homedir/orange");
    TEST_CHECK(mutt_path_pretty(path, "/homedir", true));
    TEST_CHECK_STR_EQ(buf_string(path), "~/orange");
    buf_free(&path);
  }

  // test path doesn't match homedir

  {
    struct Buffer *path = buf_new("/apple/orange");
    TEST_CHECK(!mutt_path_pretty(path, "/homedir", true));
    TEST_CHECK_STR_EQ(buf_string(path), "/apple/orange");
    buf_free(&path);
  }

  // test path matches homedir but is longer

  {
    struct Buffer *path = buf_new("/homedirnot/orange");
    TEST_CHECK(!mutt_path_pretty(path, "/homedir", true));
    TEST_CHECK_STR_EQ(buf_string(path), "/homedirnot/orange");
    buf_free(&path);
  }

  // test only homedir replacement

  {
    struct Buffer *path = buf_new("/homedir");
    TEST_CHECK(mutt_path_pretty(path, "/homedir", true));
    TEST_CHECK_STR_EQ(buf_string(path), "~");
    buf_free(&path);
  }

  // test only homedir replacement, trailing slash

  {
    struct Buffer *path = buf_new("/homedir/");
    TEST_CHECK(mutt_path_pretty(path, "/homedir", true));
    TEST_CHECK_STR_EQ(buf_string(path), "~");
    buf_free(&path);
  }
}
