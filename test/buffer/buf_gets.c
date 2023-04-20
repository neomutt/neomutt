/**
 * @file
 * Test code for buf_gets()
 *
 * @authors
 * Copyright (C) 2023 Pietro Cerutti <gahr@gahr.ch>
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

void test_buf_gets(void)
{
#define LINE1 "this is a line"
#define LINE2 "this is another line"
#define LINE3 "this is yet another line"

  char str[] = LINE1 "\n" LINE2 "\n" LINE3 "\n";
  FILE *fp = fmemopen(str, sizeof(str), "r");
  struct Buffer buf;
  buf_init(&buf);

  size_t read = 0;
  read = buf_gets(&buf, fp);
  TEST_CHECK_STR_EQ(LINE1, buf_string(&buf));
  if (!TEST_CHECK(read == strlen(LINE1)))
  {
    TEST_MSG("Expected: %zu", strlen(LINE1));
    TEST_MSG("Actual : %zu", read);
  }

  read = buf_gets(&buf, fp);
  TEST_CHECK_STR_EQ(LINE1 LINE2, buf_string(&buf));
  if (!TEST_CHECK(read == strlen(LINE2)))
  {
    TEST_MSG("Expected: %zu", strlen(LINE2));
    TEST_MSG("Actual : %zu", read);
  }

  read = buf_gets(&buf, fp);
  TEST_CHECK_STR_EQ(LINE1 LINE2 LINE3, buf_string(&buf));
  if (!TEST_CHECK(read == strlen(LINE3)))
  {
    TEST_MSG("Expected: %zu", strlen(LINE3));
    TEST_MSG("Actual : %zu", read);
  }

  buf_dealloc(&buf);
  fclose(fp);
}
