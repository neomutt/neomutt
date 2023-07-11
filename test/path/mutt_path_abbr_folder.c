/**
 * @file
 * Test code for mutt_path_abbr_folder()
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

void test_mutt_path_abbr_folder(void)
{
  // bool mutt_path_abbr_folder(char *buf, const char *folder);

  {
    TEST_CHECK(!mutt_path_abbr_folder(NULL, "apple"));
  }

  {
    struct Buffer *path = buf_new("");
    TEST_CHECK(!mutt_path_abbr_folder(path, NULL));
    buf_free(&path);
  }

  // test short folder

  {
    struct Buffer *path = buf_new("/foo/bar");
    TEST_CHECK(!mutt_path_abbr_folder(path, "/"));
    TEST_CHECK_STR_EQ(buf_string(path), "/foo/bar");
    buf_free(&path);
  }

  // test abbreviation

  {
    struct Buffer *path = buf_new("/foo/bar");
    TEST_CHECK(mutt_path_abbr_folder(path, "/foo"));
    TEST_CHECK_STR_EQ(buf_string(path), "=bar");
    buf_free(&path);
  }

  // test abbreviation folder with trailing slash

  {
    struct Buffer *path = buf_new("/foo/bar");
    TEST_CHECK(mutt_path_abbr_folder(path, "/foo/"));
    TEST_CHECK_STR_EQ(buf_string(path), "=bar");
    buf_free(&path);
  }

  // don't abbreviate without subdirectory

  {
    struct Buffer *path = buf_new("/foo/");
    TEST_CHECK(!mutt_path_abbr_folder(path, "/foo"));
    TEST_CHECK_STR_EQ(buf_string(path), "/foo/");
    buf_free(&path);
  }

  // don't abbreviate different paths

  {
    struct Buffer *path = buf_new("/foo/bar");
    TEST_CHECK(!mutt_path_abbr_folder(path, "/orange"));
    TEST_CHECK_STR_EQ(buf_string(path), "/foo/bar");
    buf_free(&path);
  }
}
