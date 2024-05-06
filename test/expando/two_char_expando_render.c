/**
 * @file
 * Test code for Two-char Expando Render
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

struct SimpleData
{
  const char *s;
  int d;
};

static void simple_ss(const struct ExpandoNode *node, void *data,
                      MuttFormatFlags flags, int max_cols, struct Buffer *buf)
{
  assert(node->type == ENT_EXPANDO);

  const struct SimpleData *sd = data;

  const char *s = NONULL(sd->s);
  buf_strcpy(buf, s);
}

static void simple_dd(const struct ExpandoNode *node, void *data,
                      MuttFormatFlags flags, int max_cols, struct Buffer *buf)
{
  assert(node->type == ENT_EXPANDO);

  const struct SimpleData *sd = data;

  const int num = sd->d;
  buf_printf(buf, "%d", num);
}

void test_expando_two_char_expando_render(void)
{
  const char *input = "%ss - %dd";

  struct ExpandoNode *root = NULL;
  struct ExpandoParseError error = { 0 };

  const struct ExpandoDefinition defs[] = {
    { "ss", NULL, 1, 0, 0, NULL },
    { "dd", NULL, 1, 1, 0, NULL },
    { NULL, NULL, 0, 0, 0, NULL },
  };

  node_tree_parse(&root, input, defs, &error);

  TEST_CHECK(error.position == NULL);
  check_node_expando(get_nth_node(root, 0), "ss", NULL);
  check_node_test(get_nth_node(root, 1), " - ");
  check_node_expando(get_nth_node(root, 2), "dd", NULL);

  const char *expected = "Test2 - 12";

  const struct Expando expando = {
    .string = input,
    .node = root,
  };

  const struct ExpandoRenderData render[] = {
    { 1, 0, simple_ss },
    { 1, 1, simple_dd },
    { -1, -1, NULL },
  };

  struct SimpleData data = {
    .s = "Test2",
    .d = 12,
  };

  struct Buffer *buf = buf_pool_get();
  expando_render(&expando, render, &data, MUTT_FORMAT_NO_FLAGS, buf->dsize, buf);

  TEST_CHECK_STR_EQ(buf_string(buf), expected);

  node_tree_free(&root);
  buf_pool_release(&buf);
}
