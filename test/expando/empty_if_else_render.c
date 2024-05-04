/**
 * @file
 * Test code for Expando if-else Rendering
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
#include <assert.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

struct SimpleEmptyIfElseData
{
  int c;
  int f;
};

static void simple_c(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, int max_cols, struct Buffer *buf)
{
  const struct SimpleEmptyIfElseData *sd = data;

  const int num = sd->c;
  if (num == 0)
    return;

  buf_printf(buf, "%d", num);
}

static void simple_f(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, int max_cols, struct Buffer *buf)
{
  assert(node->type == ENT_EXPANDO);

  const struct SimpleEmptyIfElseData *sd = data;

  const int num = sd->f;
  buf_printf(buf, "%d", num);
}

void test_expando_empty_if_else_render(void)
{
  struct ExpandoParseError error = { 0 };

  const char *input = "%<c?&%f>";

  const struct ExpandoDefinition defs[] = {
    { "c", NULL, 1, 0, 0, NULL },
    { "f", NULL, 1, 1, 0, NULL },
    { NULL, NULL, 0, 0, 0, NULL },
  };

  struct ExpandoNode *root = NULL;
  node_tree_parse(&root, input, defs, &error);
  TEST_CHECK(error.position == NULL);

  struct ExpandoNode *node = get_nth_node(root, 0);
  check_node_cond(node);

  struct ExpandoNode *condition = node_get_child(node, ENC_CONDITION);
  struct ExpandoNode *if_true_tree = node_get_child(node, ENC_TRUE);
  struct ExpandoNode *if_false_tree = node_get_child(node, ENC_FALSE);

  check_node_condbool(condition, "c");
  check_node_empty(if_true_tree);
  check_node_expando(if_false_tree, "f", NULL);

  const struct Expando expando = {
    .string = input,
    .node = root,
  };

  const struct ExpandoRenderData render[] = {
    { 1, 0, simple_c },
    { 1, 1, simple_f },
    { -1, -1, NULL },
  };

  struct SimpleEmptyIfElseData data1 = {
    .c = 0,
    .f = 3,
  };

  struct Buffer *buf = buf_pool_get();
  expando_render(&expando, render, &data1, MUTT_FORMAT_NO_FLAGS, buf->dsize, buf);

  const char *expected1 = "3";
  TEST_CHECK_STR_EQ(buf_string(buf), expected1);

  struct SimpleEmptyIfElseData data2 = {
    .c = 1,
    .f = 3,
  };

  buf_reset(buf);
  expando_render(&expando, render, &data2, MUTT_FORMAT_NO_FLAGS, buf->dsize, buf);

  const char *expected2 = "";
  TEST_CHECK_STR_EQ(buf_string(buf), expected2);

  node_tree_free(&root);
  buf_pool_release(&buf);
}
