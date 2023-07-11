/**
 * @file
 * Test code for mutt_path_to_absolute()
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

void test_mutt_path_to_absolute(void)
{
  // int mutt_path_to_absolute(char *path, const char *reference);

  MuttLogger = log_disp_null;

  {
    TEST_CHECK(!mutt_path_to_absolute(NULL, "apple"));
  }

  {
    TEST_CHECK(!mutt_path_to_absolute("apple", NULL));
  }

  {
    TEST_CHECK(mutt_path_to_absolute("/apple", "banana"));
  }

  {
    // A real dir
    char path[PATH_MAX] = { 0 };
    char reference[PATH_MAX] = { 0 };
    char expected[PATH_MAX] = { 0 };

    const char *test_dir = get_test_dir();
    const char *relative = "banana";

    strncpy(path, relative, sizeof(path));
    snprintf(reference, sizeof(reference), "%s/maildir/apple", test_dir);
    snprintf(expected, sizeof(expected), "%s/maildir/%s", test_dir, relative);

    TEST_CHECK(mutt_path_to_absolute(path, reference));
    TEST_CHECK_STR_EQ(path, expected);
  }

  {
    // A symlink
    char path[PATH_MAX] = { 0 };
    char reference[PATH_MAX] = { 0 };
    char expected[PATH_MAX] = { 0 };

    const char *test_dir = get_test_dir();
    const char *relative = "banana";

    strncpy(path, relative, sizeof(path));
    snprintf(reference, sizeof(reference), "%s/notmuch/symlink", test_dir);
    snprintf(expected, sizeof(expected), "%s/notmuch/%s", test_dir, relative);

    TEST_CHECK(mutt_path_to_absolute(path, reference));
    TEST_CHECK_STR_EQ(path, expected);
  }

  {
    // Unreadable dir
    char path[PATH_MAX] = { 0 };
    char reference[PATH_MAX] = { 0 };
    char expected[PATH_MAX] = { 0 };

    const char *test_dir = get_test_dir();
    const char *relative = "tmp";

    strncpy(path, relative, sizeof(path));
    snprintf(reference, sizeof(reference), "%s/maildir/damson/cur", test_dir);
    snprintf(expected, sizeof(expected), "%s/maildir/damson/%s", test_dir, relative);

    // We don't check the return value here.
    // If the tests are run as root, like under GitHub Actions, then realpath() will succeed.
    // If they're run as a non-root user, realpath() will fail.
    mutt_path_to_absolute(path, reference);
    TEST_CHECK_STR_EQ(path, expected);
  }
}
