/**
 * @file
 * Test code for Nested if-else Rendering
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
#include "mutt/lib.h"
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

struct NestedIfElseData
{
  int x;
  int y;
};

static void nested_x(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, int max_cols, struct Buffer *buf)
{
  const struct NestedIfElseData *sd = data;

  const int num = sd->x;
  if (num == 0)
    return;

  buf_printf(buf, "%d", num);
}

static void nested_y(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, int max_cols, struct Buffer *buf)
{
  const struct NestedIfElseData *sd = data;

  const int num = sd->y;
  if (num == 0)
    return;

  buf_printf(buf, "%d", num);
}

void test_expando_nested_if_else_render(void)
{
  const char *input = "%<x?%<y?XY&X>&%<y?Y&NONE>>";
  struct ExpandoParseError err = { 0 };
  struct ExpandoNode *root = NULL;

  const struct ExpandoDefinition defs[] = {
    { "x", NULL, 1, 0, 0, NULL },
    { "y", NULL, 1, 1, 0, NULL },
    { NULL, NULL, 0, 0, 0, NULL },
  };

  node_tree_parse(&root, input, defs, &err);

  TEST_CHECK(err.position == NULL);

  struct ExpandoNode *node = get_nth_node(root, 0);
  check_node_cond(node);

  struct ExpandoNode *condition = node_get_child(node, ENC_CONDITION);
  struct ExpandoNode *if_true_tree = node_get_child(node, ENC_TRUE);
  struct ExpandoNode *if_false_tree = node_get_child(node, ENC_FALSE);

  check_node_condbool(condition, "x");

  struct ExpandoNode *t = if_true_tree;
  check_node_cond(t);

  struct ExpandoNode *f = if_false_tree;
  check_node_cond(f);

  condition = node_get_child(t, ENC_CONDITION);
  if_true_tree = node_get_child(t, ENC_TRUE);
  if_false_tree = node_get_child(t, ENC_FALSE);

  check_node_condbool(condition, "y");
  check_node_test(if_true_tree, "XY");
  check_node_test(if_false_tree, "X");

  condition = node_get_child(f, ENC_CONDITION);
  if_true_tree = node_get_child(f, ENC_TRUE);
  if_false_tree = node_get_child(f, ENC_FALSE);

  check_node_condbool(condition, "y");
  check_node_test(if_true_tree, "Y");
  check_node_test(if_false_tree, "NONE");

  const struct Expando expando = {
    .string = input,
    .node = root,
  };

  const struct ExpandoRenderData render[] = {
    { 1, 0, nested_x },
    { 1, 1, nested_y },
    { -1, -1, NULL },
  };

  const char *expected_X = "X";
  struct NestedIfElseData data_X = {
    .x = 1,
    .y = 0,
  };

  struct Buffer *buf = buf_pool_get();
  expando_render(&expando, render, &data_X, MUTT_FORMAT_NO_FLAGS, buf->dsize, buf);

  TEST_CHECK_STR_EQ(buf_string(buf), expected_X);

  const char *expected_Y = "Y";
  struct NestedIfElseData data_Y = {
    .x = 0,
    .y = 1,
  };

  buf_reset(buf);
  expando_render(&expando, render, &data_Y, MUTT_FORMAT_NO_FLAGS, buf->dsize, buf);

  TEST_CHECK_STR_EQ(buf_string(buf), expected_Y);

  const char *expected_XY = "XY";
  struct NestedIfElseData data_XY = {
    .x = 1,
    .y = 1,
  };

  buf_reset(buf);
  expando_render(&expando, render, &data_XY, MUTT_FORMAT_NO_FLAGS, buf->dsize, buf);

  TEST_CHECK_STR_EQ(buf_string(buf), expected_XY);

  const char *expected_NONE = "NONE";
  struct NestedIfElseData data_NONE = {
    .x = 0,
    .y = 0,
  };

  buf_reset(buf);
  expando_render(&expando, render, &data_NONE, MUTT_FORMAT_NO_FLAGS, buf->dsize, buf);

  TEST_CHECK_STR_EQ(buf_string(buf), expected_NONE);

  node_tree_free(&root);
  buf_pool_release(&buf);
}
