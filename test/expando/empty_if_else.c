/**
 * @file
 * Test code for Empty if-else Expandos
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

void test_expando_empty_if_else(void)
{
  static const struct ExpandoDefinition TestFormatDef[] = {
    // clang-format off
    { "c", "cherry",    1, 2, NULL },
    { "f", "fig",       1, 2, NULL },
    { "t", "tangerine", 1, 3, NULL },
    { NULL, NULL, 0, -1, NULL }
    // clang-format on
  };

  struct Buffer *err = buf_pool_get();

  const char *input1 = "%<c?>";
  struct Expando *exp = expando_parse(input1, TestFormatDef, err);
  TEST_CHECK(exp != NULL);

  TEST_CHECK(buf_is_empty(err));
  {
    struct ExpandoNode *node_cond = node_get_child(exp->node, ENC_CONDITION);
    struct ExpandoNode *node_true = node_get_child(exp->node, ENC_TRUE);
    struct ExpandoNode *node_false = node_get_child(exp->node, ENC_FALSE);

    check_node_condbool(node_cond);
    TEST_CHECK(node_true == NULL);
    TEST_CHECK(node_false == NULL);
  }
  expando_free(&exp);

  const char *input2 = "%<c?&>";
  exp = expando_parse(input2, TestFormatDef, err);
  TEST_CHECK(exp != NULL);
  TEST_CHECK(buf_is_empty(err));
  {
    struct ExpandoNode *node_cond = node_get_child(exp->node, ENC_CONDITION);
    struct ExpandoNode *node_true = node_get_child(exp->node, ENC_TRUE);
    struct ExpandoNode *node_false = node_get_child(exp->node, ENC_FALSE);

    check_node_condbool(node_cond);
    TEST_CHECK(node_true == NULL);
    TEST_CHECK(node_false == NULL);
  }
  expando_free(&exp);

  const char *input3 = "%<c?%t&>";
  exp = expando_parse(input3, TestFormatDef, err);
  TEST_CHECK(exp != NULL);
  TEST_CHECK(buf_is_empty(err));
  {
    struct ExpandoNode *node_cond = node_get_child(exp->node, ENC_CONDITION);
    struct ExpandoNode *node_true = node_get_child(exp->node, ENC_TRUE);
    struct ExpandoNode *node_false = node_get_child(exp->node, ENC_FALSE);

    check_node_condbool(node_cond);
    check_node_expando(node_true, NULL, NULL);
    TEST_CHECK(node_false == NULL);
  }
  expando_free(&exp);

  const char *input4 = "%<c?&%f>";
  exp = expando_parse(input4, TestFormatDef, err);
  TEST_CHECK(exp != NULL);
  TEST_CHECK(buf_is_empty(err));
  {
    struct ExpandoNode *node_cond = node_get_child(exp->node, ENC_CONDITION);
    struct ExpandoNode *node_true = node_get_child(exp->node, ENC_TRUE);
    struct ExpandoNode *node_false = node_get_child(exp->node, ENC_FALSE);

    check_node_condbool(node_cond);
    TEST_CHECK(node_true == NULL);
    check_node_expando(node_false, NULL, NULL);
  }
  expando_free(&exp);
  buf_pool_release(&err);
}
