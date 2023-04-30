/**
 * @file
 * Test code for envlist_unset()
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

extern char *test_env_empty[];
extern char *test_env_one[];
extern char *test_env_five[];

void test_envlist_unset(void)
{
  bool envlist_unset(char ***envp, const char *name);

  {
    // Degenerate tests
    char **env = NULL;
    TEST_CHECK(!envlist_unset(NULL, "apple"));
    TEST_CHECK(!envlist_unset(&env, "apple"));

    env = envlist_init(test_env_empty);
    TEST_CHECK(!envlist_unset(&env, NULL));
    TEST_CHECK(!envlist_unset(&env, ""));
    envlist_free(&env);
  }

  // remove non-existant key
  {
    char **env = envlist_init(test_env_empty);
    TEST_CHECK(!envlist_unset(&env, "fig"));

    TEST_CHECK(env[0] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_one);
    TEST_CHECK(!envlist_unset(&env, "fig"));

    TEST_CHECK_STR_EQ(env[0], "apple=42");
    TEST_CHECK(env[1] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_five);
    TEST_CHECK(!envlist_unset(&env, "fig"));

    TEST_CHECK_STR_EQ(env[0], "apple=42");
    TEST_CHECK_STR_EQ(env[1], "banana=99");
    TEST_CHECK_STR_EQ(env[2], "cherry=123");
    TEST_CHECK_STR_EQ(env[3], "damson=456");
    TEST_CHECK_STR_EQ(env[4], "elder=777");
    TEST_CHECK(env[5] == NULL);

    envlist_free(&env);
  }

  // remove existing key
  {
    char **env = envlist_init(test_env_one);
    TEST_CHECK(envlist_unset(&env, "apple"));

    TEST_CHECK(env[0] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_five);
    TEST_CHECK(envlist_unset(&env, "apple"));

    TEST_CHECK_STR_EQ(env[0], "banana=99");
    TEST_CHECK_STR_EQ(env[1], "cherry=123");
    TEST_CHECK_STR_EQ(env[2], "damson=456");
    TEST_CHECK_STR_EQ(env[3], "elder=777");
    TEST_CHECK(env[4] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_five);
    TEST_CHECK(envlist_unset(&env, "banana"));

    TEST_CHECK_STR_EQ(env[0], "apple=42");
    TEST_CHECK_STR_EQ(env[1], "cherry=123");
    TEST_CHECK_STR_EQ(env[2], "damson=456");
    TEST_CHECK_STR_EQ(env[3], "elder=777");
    TEST_CHECK(env[4] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_five);
    TEST_CHECK(envlist_unset(&env, "elder"));

    TEST_CHECK_STR_EQ(env[0], "apple=42");
    TEST_CHECK_STR_EQ(env[1], "banana=99");
    TEST_CHECK_STR_EQ(env[2], "cherry=123");
    TEST_CHECK_STR_EQ(env[3], "damson=456");
    TEST_CHECK(env[4] == NULL);

    envlist_free(&env);
  }
}
