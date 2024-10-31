/**
 * @file
 * Test code for New Expando if-else
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

void test_expando_new_if_else(void)
{
  static const struct ExpandoDefinition TestFormatDef[] = {
    // clang-format off
    { "l",  "lime",   1, 1, E_TYPE_STRING, NULL },
    { "c",  "cherry", 1, 2, E_TYPE_STRING, NULL },
    { NULL, NULL, 0, -1, -1, NULL }
    // clang-format on
  };
  const char *input = "if: %<l?%4l>  if-else: %<l?%4l&%4c>";

  struct Buffer *err = buf_pool_get();
  struct Expando *exp = expando_parse(input, TestFormatDef, err);
  TEST_CHECK(exp != NULL);
  TEST_CHECK(buf_is_empty(err));

  check_node_text(node_get_child(exp->node, 0), "if: ");

  {
    struct ExpandoNode *node = node_get_child(exp->node, 1);
    check_node_cond(node);

    struct ExpandoNode *node_cond = node_get_child(node, ENC_CONDITION);
    struct ExpandoNode *node_true = node_get_child(node, ENC_TRUE);
    struct ExpandoNode *node_false = node_get_child(node, ENC_FALSE);

    check_node_condbool(node_cond);

    struct ExpandoFormat fmt = { 0 };
    fmt.min_cols = 4;
    fmt.max_cols = -1;
    fmt.justification = JUSTIFY_RIGHT;
    fmt.leader = ' ';
    check_node_expando(node_true, NULL, &fmt);
    TEST_CHECK(node_false == NULL);
  }

  check_node_text(node_get_child(exp->node, 2), "  if-else: ");

  {
    struct ExpandoNode *node = node_get_child(exp->node, 3);
    check_node_cond(node);

    struct ExpandoNode *node_cond = node_get_child(node, ENC_CONDITION);
    struct ExpandoNode *node_true = node_get_child(node, ENC_TRUE);
    struct ExpandoNode *node_false = node_get_child(node, ENC_FALSE);

    check_node_condbool(node_cond);

    struct ExpandoFormat fmt = { 0 };
    fmt.min_cols = 4;
    fmt.max_cols = -1;
    fmt.justification = JUSTIFY_RIGHT;
    fmt.leader = ' ';
    check_node_expando(node_true, NULL, &fmt);
    check_node_expando(node_false, NULL, &fmt);
  }

  expando_free(&exp);
  buf_pool_release(&err);
}
