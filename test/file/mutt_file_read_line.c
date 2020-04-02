/**
 * @file
 * Test code for mutt_file_read_line()
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

void test_mutt_file_read_line(void)
{
  // char *mutt_file_read_line(char *line, size_t *size, FILE *fp, int *line_num, int flags);

  {
    size_t size = 0;
    FILE *fp = fopen("/dev/null", "r");
    int line_num = 0;
    TEST_CHECK(!mutt_file_read_line(NULL, &size, fp, &line_num, 0));
    fclose(fp);
  }

  {
    FILE fp = { 0 };
    char *line = strdup("apple");
    int line_num = 0;
    TEST_CHECK(!mutt_file_read_line(line, NULL, &fp, &line_num, 0));
    free(line);
  }

  {
    size_t size = 0;
    char *line = strdup("apple");
    int line_num = 0;
    TEST_CHECK(!mutt_file_read_line(line, &size, NULL, &line_num, 0));
    free(line);
  }

  {
    size_t size = 0;
    char *line = strdup("apple");
    FILE fp = { 0 };
    TEST_CHECK(!mutt_file_read_line(line, &size, &fp, NULL, 0));
  }
}
