/**
 * @file
 * Test code for mutt_str_dequote_comment()
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

void test_mutt_str_dequote_comment(void)
{
  // void mutt_str_dequote_comment(char *s);

  {
    mutt_str_dequote_comment(NULL);
    TEST_CHECK_(1, "mutt_str_dequote_comment(NULL)");
  }

  {
    const char *str = "hello";
    char *dequote = strdup(str);
    mutt_str_dequote_comment(dequote);
    TEST_CHECK_(1, "mutt_str_dequote_comment(dequote)");
    TEST_CHECK(strcmp(dequote, str) == 0);
    FREE(&dequote);
  }

  {
    const char *str = "he\"ll\"o";
    const char *expected = "hello";
    char *dequote = strdup(str);
    mutt_str_dequote_comment(dequote);
    TEST_CHECK_(1, "mutt_str_dequote_comment(dequote)");
    TEST_CHECK(strcmp(dequote, expected) == 0);
    FREE(&dequote);
  }

  {
    const char *str = "he\\ll\\o";
    const char *expected = "hello";
    char *dequote = strdup(str);
    mutt_str_dequote_comment(dequote);
    TEST_CHECK_(1, "mutt_str_dequote_comment(dequote)");
    TEST_CHECK(strcmp(dequote, expected) == 0);
    FREE(&dequote);
  }

  {
    const char *str = "he\\llo\\";
    const char *expected = "hello";
    char *dequote = strdup(str);
    mutt_str_dequote_comment(dequote);
    TEST_CHECK_(1, "mutt_str_dequote_comment(dequote)");
    TEST_CHECK(strcmp(dequote, expected) == 0);
    FREE(&dequote);
  }
}
