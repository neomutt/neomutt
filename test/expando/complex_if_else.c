/**
 * @file
 * Test code for Complex Expando if-else
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
#include <limits.h>
#include <stddef.h>
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep

void test_expando_complex_if_else(void)
{
  static const struct ExpandoDefinition TestFormatDef[] = {
    // clang-format off
    { "l",  "lime",   1, 1, E_TYPE_STRING, NULL },
    { "c",  "cherry", 1, 2, E_TYPE_STRING, NULL },
    { NULL, NULL, 0, -1, -1, NULL }
    // clang-format on
  };
  const char *input = "if: %<l?pre %4lpost> if-else: %<l?pre %4lpost&pre %4cpost>";
  struct ExpandoParseError error = { 0 };
  struct ExpandoNode *root = NULL;

  node_tree_parse(&root, input, TestFormatDef, &error);

  TEST_CHECK(error.position == NULL);
  check_node_test(get_nth_node(root, 0), "if: ");

  {
    struct ExpandoNode *node = get_nth_node(root, 1);
    check_node_cond(node);

    struct ExpandoNode *condition = node_get_child(node, ENC_CONDITION);
    struct ExpandoNode *if_true_tree = node_get_child(node, ENC_TRUE);
    struct ExpandoNode *if_false_tree = node_get_child(node, ENC_FALSE);

    check_node_condbool(condition, "l");

    struct ExpandoFormat fmt = { 0 };
    fmt.min_cols = 4;
    fmt.max_cols = INT_MAX;
    fmt.justification = JUSTIFY_RIGHT;
    fmt.leader = ' ';

    check_node_test(get_nth_node(if_true_tree, 0), "pre ");
    check_node_expando(get_nth_node(if_true_tree, 1), "l", &fmt);
    check_node_test(get_nth_node(if_true_tree, 2), "post");

    TEST_CHECK(if_false_tree == NULL);
  }

  check_node_test(get_nth_node(root, 2), " if-else: ");

  {
    struct ExpandoNode *node = get_nth_node(root, 3);
    check_node_cond(node);

    struct ExpandoNode *condition = node_get_child(node, ENC_CONDITION);
    struct ExpandoNode *if_true_tree = node_get_child(node, ENC_TRUE);
    struct ExpandoNode *if_false_tree = node_get_child(node, ENC_FALSE);

    check_node_condbool(condition, "l");

    struct ExpandoFormat fmt = { 0 };
    fmt.min_cols = 4;
    fmt.max_cols = INT_MAX;
    fmt.justification = JUSTIFY_RIGHT;
    fmt.leader = ' ';

    check_node_test(get_nth_node(if_true_tree, 0), "pre ");
    check_node_expando(get_nth_node(if_true_tree, 1), "l", &fmt);
    check_node_test(get_nth_node(if_true_tree, 2), "post");

    check_node_test(get_nth_node(if_false_tree, 0), "pre ");
    check_node_expando(get_nth_node(if_false_tree, 1), "c", &fmt);
    check_node_test(get_nth_node(if_false_tree, 2), "post");
  }

  node_tree_free(&root);
}
