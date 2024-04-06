/**
 * @file
 * Test code for Simple Expando
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
#include <stddef.h>
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep

void test_expando_simple_expando(void)
{
  static const struct ExpandoDefinition TestFormatDef[] = {
    // clang-format off
    { "a",  "apple",  1, 1, E_TYPE_STRING, NULL },
    { "b",  "banana", 1, 2, E_TYPE_STRING, NULL },
    { NULL, NULL, 0, -1, -1, NULL }
    // clang-format on
  };
  const char *input = "%a %b";

  struct Buffer *err = buf_pool_get();
  struct Expando *exp = expando_parse(input, TestFormatDef, err);
  TEST_CHECK(exp != NULL);
  TEST_CHECK(buf_is_empty(err));

  check_node_expando(node_get_child(exp->node, 0), "a", NULL);
  check_node_test(node_get_child(exp->node, 1), " ");
  check_node_expando(node_get_child(exp->node, 2), "b", NULL);

  expando_free(&exp);
  buf_pool_release(&err);
}
