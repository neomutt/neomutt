/**
 * @file
 * Test code for mutt_path_tilde()
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
#include <pwd.h>
#include <sys/types.h>
#include "mutt/lib.h"
#include "test_common.h"

void test_mutt_path_tilde(void)
{
  // bool mutt_path_tilde(struct Buffer *path, const char *homedir);

  {
    TEST_CHECK(!mutt_path_tilde(NULL, "/homedir"));
  }

  // test no tilde

  {
    struct Buffer *path = buf_new("/orange");
    TEST_CHECK(!mutt_path_tilde(path, NULL));
    TEST_CHECK_STR_EQ(buf_string(path), "/orange");
    buf_free(&path);
  }

  // test no homedir

  {
    struct Buffer *path = buf_new("~/orange");
    TEST_CHECK(!mutt_path_tilde(path, NULL));
    buf_free(&path);
  }

  // test homedir expansion

  {
    struct Buffer *path = buf_new("~/orange");
    TEST_CHECK(mutt_path_tilde(path, "/homedir"));
    TEST_CHECK_STR_EQ(buf_string(path), "/homedir/orange");
    buf_free(&path);
  }

  // test homedir expansion without subdirectory

  {
    struct Buffer *path = buf_new("~");
    TEST_CHECK(mutt_path_tilde(path, "/homedir"));
    TEST_CHECK_STR_EQ(buf_string(path), "/homedir");
    buf_free(&path);
  }

  // test user expansion

  {
    struct passwd *pw = getpwnam("root");
    TEST_CHECK(pw != NULL);
    TEST_CHECK(pw->pw_dir != NULL);

    struct Buffer *expected = buf_new(NULL);
    buf_printf(expected, "%s/orange", pw->pw_dir);

    struct Buffer *path = buf_new("~root/orange");
    TEST_CHECK(mutt_path_tilde(path, NULL));
    TEST_CHECK_STR_EQ(buf_string(path), buf_string(expected));

    buf_free(&expected);
    buf_free(&path);
  }

  // test non-user expansion

  {
    struct Buffer *path = buf_new("~hopefullydoesnotexist/orange");
    TEST_CHECK(!mutt_path_tilde(path, NULL));
    TEST_CHECK_STR_EQ(buf_string(path), "~hopefullydoesnotexist/orange");
    buf_free(&path);
  }

  // test non-user expansion without subdirectory

  {
    struct Buffer *path = buf_new("~hopefullydoesnotexist");
    TEST_CHECK(!mutt_path_tilde(path, NULL));
    TEST_CHECK_STR_EQ(buf_string(path), "~hopefullydoesnotexist");
    buf_free(&path);
  }
}
