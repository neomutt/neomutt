/**
 * @file
 * Test code for mutt_file_map_lines()
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
#include "common.h"

#define BOOLIFY(x) ((x) ? "true" : "false")

bool map_dummy(char *line, int line_num, void *user_data)
{
  return false;
}

static bool mapping_func(char *line, int line_num, void *user_data)
{
  const int *p_last_line_num = (const int *) (user_data);
  if (!TEST_CHECK(strcmp(line, file_lines[line_num - 1]) == 0))
  {
    TEST_MSG("Expected: %s", file_lines[line_num - 1]);
    TEST_MSG("Actual: %s", line);
  }
  return (line_num < *p_last_line_num);
}

static void test_file_map_lines_breaking_after(int last_line, bool expected)
{
  FILE *fp = SET_UP();
  if (!fp)
    return;
  bool res = mutt_file_map_lines(mapping_func, &last_line, fp, 0);
  if (!TEST_CHECK(res == expected))
  {
    TEST_MSG("Expected: %s", BOOLIFY(expected));
    TEST_MSG("Actual: %s", BOOLIFY(res));
  }
  TEAR_DOWN(fp);
}

void test_mutt_file_map_lines(void)
{
  // bool mutt_file_map_lines(mutt_file_map_t func, void *user_data, FILE *fp, int flags);

  {
    FILE fp = { 0 };
    TEST_CHECK(!mutt_file_map_lines(NULL, "apple", &fp, 0));
  }

  {
    mutt_file_map_t map = map_dummy;
    FILE *fp = fopen("/dev/null", "r");
    TEST_CHECK(mutt_file_map_lines(map, NULL, fp, 0));
    fclose(fp);
  }

  {
    mutt_file_map_t map = map_dummy;
    TEST_CHECK(!mutt_file_map_lines(map, "apple", NULL, 0));
  }

  {
    const size_t num = file_num_test_lines();
    test_file_map_lines_breaking_after(num + 1, true);
    test_file_map_lines_breaking_after(0, false);
    test_file_map_lines_breaking_after(1, false);
    test_file_map_lines_breaking_after(num, false);
  }
}
