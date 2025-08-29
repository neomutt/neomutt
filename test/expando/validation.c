/**
 * @file
 * Test code for Expando Validation
 *
 * @authors
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
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
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep

void test_expando_validation(void)
{
  const char *input1 = "%a";
  const char *input2 = "%a";

  const struct ExpandoDefinition defs1[] = {
    { "a", NULL, 1, 0, NULL },
    { NULL, NULL, 0, 0, NULL },
  };

  const struct ExpandoDefinition defs2[] = {
    { "b", NULL, 1, 0, NULL },
    { NULL, NULL, 0, 0, NULL },
  };

  struct Buffer *err = buf_new(NULL);
  buf_alloc(err, 128);

  struct Expando *null1 = expando_parse(NULL, defs1, err);
  TEST_CHECK(null1 == NULL);

  struct Expando *null2 = expando_parse(input1, NULL, err);
  TEST_CHECK(null2 == NULL);

  struct Expando *valid1 = expando_parse(input1, defs1, err);
  TEST_CHECK(valid1 != NULL);

  const bool equals1 = expando_equal(null1, null2);
  TEST_CHECK(equals1);

  const bool equals2 = expando_equal(null1, valid1);
  TEST_CHECK(!equals2);

  struct Expando *valid2 = expando_parse(input2, defs1, err);
  TEST_CHECK(valid2 != NULL);

  const bool equals3 = expando_equal(valid1, valid2);
  TEST_CHECK(equals3);

  struct Expando *invalid1 = expando_parse(input2, defs2, err);
  TEST_CHECK(invalid1 == NULL);

  expando_free(&valid1);
  expando_free(&valid2);

  buf_free(&err);
}
