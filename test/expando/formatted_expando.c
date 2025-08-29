/**
 * @file
 * Test code for Formatting Expandos
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
#include "mutt/lib.h"
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep

void test_expando_formatted_expando(void)
{
  static const struct ExpandoDefinition TestFormatDef[] = {
    // clang-format off
    { "X",  "xigua", 1, 1, NULL },
    { NULL, NULL, 0, -1, NULL }
    // clang-format on
  };
  const char *input = "%X %8X %-8X %08X %.8X %8.8X %-8.8X %=8X";

  struct Buffer *err = buf_pool_get();

  struct Expando *exp = expando_parse(input, TestFormatDef, err);
  TEST_CHECK(exp != NULL);
  TEST_CHECK(buf_is_empty(err));

  // "%X"
  check_node_expando(node_get_child(exp->node, 0), NULL, NULL);
  check_node_text(node_get_child(exp->node, 1), " ");

  {
    // "%8X"
    struct ExpandoFormat fmt = { 0 };
    fmt.min_cols = 8;
    fmt.max_cols = -1;
    fmt.justification = JUSTIFY_RIGHT;
    fmt.leader = ' ';
    check_node_expando(node_get_child(exp->node, 2), NULL, &fmt);
    check_node_text(node_get_child(exp->node, 3), " ");
  }

  {
    // "%-8X"
    struct ExpandoFormat fmt = { 0 };
    fmt.min_cols = 8;
    fmt.max_cols = -1;
    fmt.justification = JUSTIFY_LEFT;
    fmt.leader = ' ';
    check_node_expando(node_get_child(exp->node, 4), NULL, &fmt);
    check_node_text(node_get_child(exp->node, 5), " ");
  }

  {
    // "%08X"
    struct ExpandoFormat fmt = { 0 };
    fmt.min_cols = 8;
    fmt.max_cols = -1;
    fmt.justification = JUSTIFY_RIGHT;
    fmt.leader = '0';
    check_node_expando(node_get_child(exp->node, 6), NULL, &fmt);
    check_node_text(node_get_child(exp->node, 7), " ");
  }

  {
    // "%.8X"
    struct ExpandoFormat fmt = { 0 };
    fmt.min_cols = 0;
    fmt.max_cols = 8;
    fmt.justification = JUSTIFY_RIGHT;
    fmt.leader = '0';
    check_node_expando(node_get_child(exp->node, 8), NULL, &fmt);
    check_node_text(node_get_child(exp->node, 9), " ");
  }

  {
    // "%8.8X"
    struct ExpandoFormat fmt = { 0 };
    fmt.min_cols = 8;
    fmt.max_cols = 8;
    fmt.justification = JUSTIFY_RIGHT;
    fmt.leader = '0';
    check_node_expando(node_get_child(exp->node, 10), NULL, &fmt);
    check_node_text(node_get_child(exp->node, 11), " ");
  }

  {
    // "%-8.8X"
    struct ExpandoFormat fmt = { 0 };
    fmt.min_cols = 8;
    fmt.max_cols = 8;
    fmt.justification = JUSTIFY_LEFT;
    fmt.leader = '0';
    check_node_expando(node_get_child(exp->node, 12), NULL, &fmt);
    check_node_text(node_get_child(exp->node, 13), " ");
  }

  {
    // "%=8X"
    struct ExpandoFormat fmt = { 0 };
    fmt.min_cols = 8;
    fmt.max_cols = -1;
    fmt.justification = JUSTIFY_CENTER;
    fmt.leader = ' ';
    check_node_expando(node_get_child(exp->node, 14), NULL, &fmt);
  }

  expando_free(&exp);
  buf_pool_release(&err);
}
