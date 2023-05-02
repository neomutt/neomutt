/**
 * @file
 * Test code for envlist_init()
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
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

char *test_env_empty[] = {
  NULL,
};

char *test_env_one[] = {
  "apple=42",
  NULL,
};

char *test_env_five[] = {
  "apple=42", "banana=99", "cherry=123", "damson=456", "elder=777", NULL,
};

void test_envlist_init(void)
{
  // char **envlist_init(char **envp);

  {
    char **env = envlist_init(NULL);
    TEST_CHECK(env == NULL);
  }

  {
    char **env = envlist_init(test_env_empty);
    TEST_CHECK(env != NULL);

    TEST_CHECK(env[0] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_one);
    TEST_CHECK(env != NULL);

    TEST_CHECK_STR_EQ(env[0], "apple=42");
    TEST_CHECK(env[1] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_five);
    TEST_CHECK(env != NULL);

    TEST_CHECK_STR_EQ(env[0], "apple=42");
    TEST_CHECK_STR_EQ(env[1], "banana=99");
    TEST_CHECK_STR_EQ(env[2], "cherry=123");
    TEST_CHECK_STR_EQ(env[3], "damson=456");
    TEST_CHECK_STR_EQ(env[4], "elder=777");
    TEST_CHECK(env[5] == NULL);

    envlist_free(&env);
  }
}
