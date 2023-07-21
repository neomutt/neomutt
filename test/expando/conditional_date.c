/**
 * @file
 * Test code for Conditional Date Expandos
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
#include <stdio.h>
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep

void test_expando_conditional_date(void)
{
  const char *input = "%<[1m?%[%d-%m-%Y]&%[%Y-%m-%d]>";

  struct ExpandoNode *root = NULL;
  struct ExpandoParseError error = { 0 };

  const struct ExpandoDefinition defs[] = {
    { "[", NULL, 1, 0, 0, parse_date },
    { NULL, NULL, 0, 0, 0, NULL },
  };

  node_tree_parse(&root, input, defs, &error);

  TEST_CHECK(error.position == NULL);

  struct ExpandoNode *node = get_nth_node(root, 0);
  check_node_cond(node);

  struct ExpandoNode *condition = node_get_child(node, ENC_CONDITION);
  struct ExpandoNode *if_true_tree = node_get_child(node, ENC_TRUE);
  struct ExpandoNode *if_false_tree = node_get_child(node, ENC_FALSE);

  check_node_conddate(condition, 1, 'm');
  check_node_expando(if_true_tree, "%d-%m-%Y", NULL);
  check_node_expando(if_false_tree, "%Y-%m-%d", NULL);

  node_tree_free(&root);
}
