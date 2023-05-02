/**
 * @file
 * Test code for envlist_set()
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
#include <stdbool.h>
#include "mutt/lib.h"
#include "test_common.h"

extern char *test_env_empty[];
extern char *test_env_one[];
extern char *test_env_five[];

void test_envlist_set(void)
{
  bool envlist_set(char ***envp, const char *name, const char *value, bool overwrite);

  {
    // Degenerate tests
    char **env = NULL;
    TEST_CHECK(!envlist_set(NULL, "apple", "value", false));
    TEST_CHECK(!envlist_set(&env, "apple", "value", false));

    env = envlist_init(test_env_empty);
    TEST_CHECK(!envlist_set(&env, NULL, "value", false));
    TEST_CHECK(!envlist_set(&env, "", "value", false));
    envlist_free(&env);
  }

  // insert non-existant key
  {
    char **env = envlist_init(test_env_empty);
    TEST_CHECK(envlist_set(&env, "fig", "value", false));

    TEST_CHECK_STR_EQ(env[0], "fig=value");
    TEST_CHECK(env[1] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_one);
    TEST_CHECK(envlist_set(&env, "fig", "value", false));

    TEST_CHECK_STR_EQ(env[0], "apple=42");
    TEST_CHECK_STR_EQ(env[1], "fig=value");
    TEST_CHECK(env[2] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_five);
    TEST_CHECK(envlist_set(&env, "fig", "value", false));

    TEST_CHECK_STR_EQ(env[0], "apple=42");
    TEST_CHECK_STR_EQ(env[1], "banana=99");
    TEST_CHECK_STR_EQ(env[2], "cherry=123");
    TEST_CHECK_STR_EQ(env[3], "damson=456");
    TEST_CHECK_STR_EQ(env[4], "elder=777");
    TEST_CHECK_STR_EQ(env[5], "fig=value");
    TEST_CHECK(env[6] == NULL);

    envlist_free(&env);
  }

  // insert existing key - no overwrite
  {
    char **env = envlist_init(test_env_one);
    TEST_CHECK(!envlist_set(&env, "apple", "value", false));

    TEST_CHECK_STR_EQ(env[0], "apple=42");
    TEST_CHECK(env[1] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_five);
    TEST_CHECK(!envlist_set(&env, "apple", "value", false));

    TEST_CHECK_STR_EQ(env[0], "apple=42");
    TEST_CHECK_STR_EQ(env[1], "banana=99");
    TEST_CHECK_STR_EQ(env[2], "cherry=123");
    TEST_CHECK_STR_EQ(env[3], "damson=456");
    TEST_CHECK_STR_EQ(env[4], "elder=777");
    TEST_CHECK(env[5] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_five);
    TEST_CHECK(!envlist_set(&env, "banana", "value", false));

    TEST_CHECK_STR_EQ(env[0], "apple=42");
    TEST_CHECK_STR_EQ(env[1], "banana=99");
    TEST_CHECK_STR_EQ(env[2], "cherry=123");
    TEST_CHECK_STR_EQ(env[3], "damson=456");
    TEST_CHECK_STR_EQ(env[4], "elder=777");
    TEST_CHECK(env[5] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_five);
    TEST_CHECK(!envlist_set(&env, "damson", "value", false));

    TEST_CHECK_STR_EQ(env[0], "apple=42");
    TEST_CHECK_STR_EQ(env[1], "banana=99");
    TEST_CHECK_STR_EQ(env[2], "cherry=123");
    TEST_CHECK_STR_EQ(env[3], "damson=456");
    TEST_CHECK_STR_EQ(env[4], "elder=777");
    TEST_CHECK(env[5] == NULL);

    envlist_free(&env);
  }

  // insert existing key - overwrite
  {
    char **env = envlist_init(test_env_one);
    TEST_CHECK(envlist_set(&env, "apple", "value", true));

    TEST_CHECK_STR_EQ(env[0], "apple=value");
    TEST_CHECK(env[1] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_five);
    TEST_CHECK(envlist_set(&env, "apple", "value", true));

    TEST_CHECK_STR_EQ(env[0], "apple=value");
    TEST_CHECK_STR_EQ(env[1], "banana=99");
    TEST_CHECK_STR_EQ(env[2], "cherry=123");
    TEST_CHECK_STR_EQ(env[3], "damson=456");
    TEST_CHECK_STR_EQ(env[4], "elder=777");
    TEST_CHECK(env[5] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_five);
    TEST_CHECK(envlist_set(&env, "banana", "value", true));

    TEST_CHECK_STR_EQ(env[0], "apple=42");
    TEST_CHECK_STR_EQ(env[1], "banana=value");
    TEST_CHECK_STR_EQ(env[2], "cherry=123");
    TEST_CHECK_STR_EQ(env[3], "damson=456");
    TEST_CHECK_STR_EQ(env[4], "elder=777");
    TEST_CHECK(env[5] == NULL);

    envlist_free(&env);
  }

  {
    char **env = envlist_init(test_env_five);
    TEST_CHECK(envlist_set(&env, "elder", "value", true));

    TEST_CHECK_STR_EQ(env[0], "apple=42");
    TEST_CHECK_STR_EQ(env[1], "banana=99");
    TEST_CHECK_STR_EQ(env[2], "cherry=123");
    TEST_CHECK_STR_EQ(env[3], "damson=456");
    TEST_CHECK_STR_EQ(env[4], "elder=value");
    TEST_CHECK(env[5] == NULL);

    envlist_free(&env);
  }
}
