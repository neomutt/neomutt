/**
 * @file
 * Common code for file tests
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

const char *file_lines[] = {
  "This is the first line.",
  "The second line.",
  "And the third line",
  NULL,
};

size_t file_num_test_lines(void)
{
  return (sizeof(file_lines) / sizeof(const char *) - 1);
}

FILE *file_set_up(const char *funcname)
{
  int res = 0;
  const char **linep = NULL;
  FILE *fp = tmpfile();
  if (!fp)
    goto err1;
  for (linep = file_lines; *linep; linep++)
  {
    res = fputs(*linep, fp);
    if (res == EOF)
      goto err2;
    res = fputc('\n', fp);
    if (res == EOF)
      goto err2;
  }
  rewind(fp);
  return fp;
err2:
  fclose(fp);
err1:
  TEST_MSG("Failed to set up test %s", funcname);
  return NULL;
}

void file_tear_down(FILE *fp, const char *funcname)
{
  int res = fclose(fp);
  if (res == EOF)
    TEST_MSG("Failed to tear down test %s", funcname);
}
