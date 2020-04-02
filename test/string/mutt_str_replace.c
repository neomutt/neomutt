/**
 * @file
 * Test code for mutt_str_replace()
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
#include "mutt/lib.h"

void test_mutt_str_replace(void)
{
  // void mutt_str_replace(char **p, const char *s);

  {
    mutt_str_replace(NULL, NULL);
    TEST_CHECK_(1, "mutt_str_replace(NULL, NULL)");
  }

  {
    mutt_str_replace(NULL, "apple");
    TEST_CHECK_(1, "mutt_str_replace(NULL, \"apple\")");
  }

  {
    char *ptr = NULL;
    mutt_str_replace(&ptr, NULL);
    TEST_CHECK_(1, "mutt_str_replace(&ptr, NULL)");
  }

  {
    const char *str = "hello";
    char *ptr = NULL;
    mutt_str_replace(&ptr, str);
    TEST_CHECK(ptr != NULL);
    TEST_CHECK(ptr != str);
    TEST_CHECK(strcmp(ptr, str) == 0);
    FREE(&ptr);
  }

  {
    const char *str = "hello";
    char *ptr = strdup("bye");
    mutt_str_replace(&ptr, str);
    TEST_CHECK(ptr != NULL);
    TEST_CHECK(ptr != str);
    TEST_CHECK(strcmp(ptr, str) == 0);
    FREE(&ptr);
  }

  {
    const char *str = "";
    char *ptr = NULL;
    mutt_str_replace(&ptr, str);
    TEST_CHECK(ptr == NULL);
  }

  {
    const char *str = "";
    char *ptr = strdup("bye");
    mutt_str_replace(&ptr, str);
    TEST_CHECK(ptr == NULL);
  }

  {
    const char *str = NULL;
    char *ptr = NULL;
    mutt_str_replace(&ptr, str);
    TEST_CHECK(ptr == NULL);
  }

  {
    const char *str = NULL;
    char *ptr = strdup("bye");
    mutt_str_replace(&ptr, str);
    TEST_CHECK(ptr == NULL);
  }
}
