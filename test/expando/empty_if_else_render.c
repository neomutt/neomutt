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
  ASSERT(node->type == ENT_EXPANDO);

  const struct SimpleEmptyIfElseData *sd = data;

  const int num = sd->f;
  buf_printf(buf, "%d", num);
}

void test_expando_empty_if_else_render(void)
{
  const char *input = "%<c?&%f>";

  const struct ExpandoDefinition defs[] = {
    { "c", NULL, 1, 0, 0, NULL },
    { "f", NULL, 1, 1, 0, NULL },
    { NULL, NULL, 0, 0, 0, NULL },
  };

  struct Buffer *err = buf_pool_get();

  struct Expando *exp = expando_parse(input, defs, err);
  TEST_CHECK(exp != NULL);
  TEST_CHECK(buf_is_empty(err));

  struct ExpandoNode *condition = node_get_child(exp->node, ENC_CONDITION);
  struct ExpandoNode *if_true_tree = node_get_child(exp->node, ENC_TRUE);
  struct ExpandoNode *if_false_tree = node_get_child(exp->node, ENC_FALSE);

  check_node_condbool(condition, "c");
  TEST_CHECK(if_true_tree == NULL);
  check_node_expando(if_false_tree, "f", NULL);

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
  expando_render(exp, render, &data1, MUTT_FORMAT_NO_FLAGS, buf->dsize, buf);

  const char *expected1 = "3";
  TEST_CHECK_STR_EQ(buf_string(buf), expected1);

  struct SimpleEmptyIfElseData data2 = {
    .c = 1,
    .f = 3,
  };

  buf_reset(buf);
  expando_render(exp, render, &data2, MUTT_FORMAT_NO_FLAGS, buf->dsize, buf);

  const char *expected2 = "";
  TEST_CHECK_STR_EQ(buf_string(buf), expected2);

  expando_free(&exp);
  buf_pool_release(&buf);
  buf_pool_release(&err);
}
