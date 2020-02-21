/**
 * @file
 * Common code for file tests
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
#include <locale.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "mutt/lib.h"

#define TEST_DIR "NEOMUTT_TEST_DIR"

const char *test_get_test_dir(void)
{
  return mutt_str_getenv(TEST_DIR);
}

void test_gen_path(char *buf, size_t buflen, const char *fmt)
{
  snprintf(buf, buflen, NONULL(fmt), NONULL(test_get_test_dir()));
}

void test_init(void)
{
  const char *path = test_get_test_dir();
  bool success = false;

  TEST_CASE("Common setup");
  if (!TEST_CHECK(path != NULL))
  {
    TEST_MSG("Environment variable '%s' isn't set\n", TEST_DIR);
    goto done;
  }

  size_t len = strlen(path);
  if (!TEST_CHECK(path[len - 1] != '/'))
  {
    TEST_MSG("Environment variable '%s' mustn't end with a '/'\n", TEST_DIR);
    goto done;
  }

  struct stat st = { 0 };

  if (!TEST_CHECK(stat(path, &st) == 0))
  {
    TEST_MSG("Test dir '%s' doesn't exist\n", path);
    goto done;
  }

  if (!TEST_CHECK(S_ISDIR(st.st_mode) == true))
  {
    TEST_MSG("Test dir '%s' isn't a directory\n", path);
    goto done;
  }

  if (!TEST_CHECK((setlocale(LC_ALL, "C.UTF-8") != NULL) ||
                  (setlocale(LC_ALL, "en_US.UTF-8") != NULL)))
  {
    TEST_MSG("Can't set locale to C.UTF-8 or en_US.UTF-8");
    goto done;
  }

  success = true;
done:
  if (!success)
    TEST_MSG("See: https://github.com/neomutt/neomutt-test-files#test-files\n");
}
