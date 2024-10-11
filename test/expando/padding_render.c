/**
 * @file
 * Test code for Padding Expando Rendering
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
#include "test_common.h"

struct NullData
{
  int n;
};

void test_expando_padding_render(void)
{
  static const struct ExpandoDefinition FormatDef[] = {
    // clang-format off
    { "*", "padding-soft", ED_GLOBAL, ED_GLO_PADDING_SOFT, E_TYPE_STRING, node_padding_parse },
    { ">", "padding-hard", ED_GLOBAL, ED_GLO_PADDING_HARD, E_TYPE_STRING, node_padding_parse },
    { "|", "padding-eol",  ED_GLOBAL, ED_GLO_PADDING_EOL,  E_TYPE_STRING, node_padding_parse },
    { NULL, NULL, 0, -1, -1, NULL }
    // clang-format on
  };

  struct ExpandoParseError err = { 0 };

  {
    const char *input = "text1%|-text2";
    struct ExpandoNode *root = NULL;

    node_tree_parse(&root, input, FormatDef, &err);

    TEST_CHECK(err.position == NULL);
    check_node_padding(root, "-", EPT_FILL_EOL);

    struct ExpandoNode *left = node_get_child(root, ENP_LEFT);
    struct ExpandoNode *right = node_get_child(root, ENP_RIGHT);

    TEST_CHECK(left != NULL);
    TEST_CHECK(right != NULL);

    check_node_text(left, "text1");
    check_node_text(right, "text2");

    const struct Expando exp = {
      .string = input,
      .node = root,
    };

    const struct ExpandoRenderData render[] = {
      { -1, -1, NULL },
    };

    struct NullData data = { 0 };

    const char *expected = "text1---";
    struct Buffer *buf = buf_pool_get();
    expando_render(&exp, render, &data, MUTT_FORMAT_NO_FLAGS, 8, buf);

    TEST_CHECK_STR_EQ(buf_string(buf), expected);
    TEST_MSG("Expected: %s", expected);
    TEST_MSG("Actual:   %s", buf_string(buf));

    node_tree_free(&root);
    buf_pool_release(&buf);
  }

  {
    const char *input = "text1%|-text2";
    struct ExpandoNode *root = NULL;

    node_tree_parse(&root, input, FormatDef, &err);

    TEST_CHECK(err.position == NULL);
    check_node_padding(root, "-", EPT_FILL_EOL);

    struct ExpandoNode *left = node_get_child(root, ENP_LEFT);
    struct ExpandoNode *right = node_get_child(root, ENP_RIGHT);

    TEST_CHECK(left != NULL);
    TEST_CHECK(right != NULL);

    check_node_text(left, "text1");
    check_node_text(right, "text2");

    const struct Expando exp = {
      .string = input,
      .node = root,
    };

    const struct ExpandoRenderData render[] = {
      { -1, -1, NULL },
    };

    struct NullData data = { 0 };

    const char *expected = "text1--------";
    struct Buffer *buf = buf_pool_get();
    expando_render(&exp, render, &data, MUTT_FORMAT_NO_FLAGS, 13, buf);

    TEST_CHECK_STR_EQ(buf_string(buf), expected);

    node_tree_free(&root);
    buf_pool_release(&buf);
  }

  {
    const char *input = "text1%>-text2";
    struct ExpandoNode *root = NULL;

    node_tree_parse(&root, input, FormatDef, &err);

    TEST_CHECK(err.position == NULL);
    check_node_padding(root, "-", EPT_HARD_FILL);

    struct ExpandoNode *left = node_get_child(root, ENP_LEFT);
    struct ExpandoNode *right = node_get_child(root, ENP_RIGHT);

    TEST_CHECK(left != NULL);
    TEST_CHECK(right != NULL);

    check_node_text(left, "text1");
    check_node_text(right, "text2");

    const struct Expando exp = {
      .string = input,
      .node = root,
    };

    const struct ExpandoRenderData render[] = {
      { -1, -1, NULL },
    };

    struct NullData data = { 0 };

    const char *expected = "text1tex";
    struct Buffer *buf = buf_pool_get();
    expando_render(&exp, render, &data, MUTT_FORMAT_NO_FLAGS, 8, buf);
    TEST_CHECK_STR_EQ(buf_string(buf), expected);
    TEST_MSG("Expected: %s", expected);
    TEST_MSG("Actual:   %s", buf_string(buf));

    node_tree_free(&root);
    buf_pool_release(&buf);
  }

  {
    const char *input = "text1%>-text2";
    struct ExpandoNode *root = NULL;

    node_tree_parse(&root, input, FormatDef, &err);

    TEST_CHECK(err.position == NULL);
    check_node_padding(root, "-", EPT_HARD_FILL);

    struct ExpandoNode *left = node_get_child(root, ENP_LEFT);
    struct ExpandoNode *right = node_get_child(root, ENP_RIGHT);

    TEST_CHECK(left != NULL);
    TEST_CHECK(right != NULL);

    check_node_text(left, "text1");
    check_node_text(right, "text2");

    const struct Expando exp = {
      .string = input,
      .node = root,
    };

    const struct ExpandoRenderData render[] = {
      { -1, -1, NULL },
    };

    struct NullData data = { 0 };

    const char *expected = "text1---text2";
    struct Buffer *buf = buf_pool_get();
    expando_render(&exp, render, &data, MUTT_FORMAT_NO_FLAGS, 13, buf);

    TEST_CHECK_STR_EQ(buf_string(buf), expected);
    TEST_MSG("Expected: %s", expected);
    TEST_MSG("Actual:   %s", buf_string(buf));

    node_tree_free(&root);
    buf_pool_release(&buf);
  }

  {
    const char *input = "text1%*-text2";
    struct ExpandoNode *root = NULL;

    node_tree_parse(&root, input, FormatDef, &err);

    TEST_CHECK(err.position == NULL);
    check_node_padding(root, "-", EPT_SOFT_FILL);

    struct ExpandoNode *left = node_get_child(root, ENP_LEFT);
    struct ExpandoNode *right = node_get_child(root, ENP_RIGHT);

    TEST_CHECK(left != NULL);
    TEST_CHECK(right != NULL);

    check_node_text(left, "text1");
    check_node_text(right, "text2");

    const struct Expando exp = {
      .string = input,
      .node = root,
    };

    const struct ExpandoRenderData render[] = {
      { -1, -1, NULL },
    };

    struct NullData data = { 0 };

    const char *expected = "textext2";
    struct Buffer *buf = buf_pool_get();
    expando_render(&exp, render, &data, MUTT_FORMAT_NO_FLAGS, 8, buf);

    TEST_CHECK_STR_EQ(buf_string(buf), expected);
    TEST_MSG("Expected: %s", expected);
    TEST_MSG("Actual:   %s", buf_string(buf));

    node_tree_free(&root);
    buf_pool_release(&buf);
  }

  {
    const char *input = "text1%*-text2";
    struct ExpandoNode *root = NULL;

    node_tree_parse(&root, input, FormatDef, &err);

    TEST_CHECK(err.position == NULL);
    check_node_padding(root, "-", EPT_SOFT_FILL);

    struct ExpandoNode *left = node_get_child(root, ENP_LEFT);
    struct ExpandoNode *right = node_get_child(root, ENP_RIGHT);

    TEST_CHECK(left != NULL);
    TEST_CHECK(right != NULL);

    check_node_text(left, "text1");
    check_node_text(right, "text2");

    const struct Expando exp = {
      .string = input,
      .node = root,
    };

    const struct ExpandoRenderData render[] = {
      { -1, -1, NULL },
    };

    struct NullData data = { 0 };

    const char *expected = "text1---text2";
    struct Buffer *buf = buf_pool_get();
    expando_render(&exp, render, &data, MUTT_FORMAT_NO_FLAGS, 13, buf);

    TEST_CHECK_STR_EQ(buf_string(buf), expected);
    TEST_MSG("Expected: %s", expected);
    TEST_MSG("Actual:   %s", buf_string(buf));

    node_tree_free(&root);
    buf_pool_release(&buf);
  }

  {
    const char *input = "text1%*-text2";
    struct ExpandoNode *root = NULL;

    node_tree_parse(&root, input, FormatDef, &err);

    TEST_CHECK(err.position == NULL);
    check_node_padding(root, "-", EPT_SOFT_FILL);

    struct ExpandoNode *left = node_get_child(root, ENP_LEFT);
    struct ExpandoNode *right = node_get_child(root, ENP_RIGHT);

    TEST_CHECK(left != NULL);
    TEST_CHECK(right != NULL);

    check_node_text(left, "text1");
    check_node_text(right, "text2");

    const struct Expando exp = {
      .string = input,
      .node = root,
    };

    const struct ExpandoRenderData render[] = {
      { -1, -1, NULL },
    };

    struct NullData data = { 0 };

    const char *expected = "text2";
    struct Buffer *buf = buf_pool_get();
    expando_render(&exp, render, &data, MUTT_FORMAT_NO_FLAGS, 5, buf);

    TEST_CHECK_STR_EQ(buf_string(buf), expected);
    TEST_MSG("Expected: %s", expected);
    TEST_MSG("Actual:   %s", buf_string(buf));

    node_tree_free(&root);
    buf_pool_release(&buf);
  }
}
