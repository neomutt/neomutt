/**
 * @file
 * Test code for Nested if-else Expandos
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
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep

void test_expando_nested_if_else(void)
{
  static const struct ExpandoDefinition TestFormatDef[] = {
    // clang-format off
    { "a", "apple",      1, 1, E_TYPE_STRING, NULL },
    { "b", "banana",     1, 2, E_TYPE_STRING, NULL },
    { "c", "cherry",     1, 3, E_TYPE_STRING, NULL },
    { "d", "damson",     1, 4, E_TYPE_STRING, NULL },
    { "e", "elderberry", 1, 5, E_TYPE_STRING, NULL },
    { "f", "fig",        1, 6, E_TYPE_STRING, NULL },
    { "g", "guava",      1, 7, E_TYPE_STRING, NULL },
    { NULL, NULL, 0, -1, -1, NULL }
    // clang-format on
  };
  struct Buffer *err = buf_pool_get();

  {
    const char *input = "%<a?%<b?%c&%d>&%<e?%f&%g>>";

    struct Expando *exp = expando_parse(input, TestFormatDef, err);
    TEST_CHECK(exp != NULL);
    TEST_CHECK(buf_is_empty(err));

    struct ExpandoNode *node_cond = node_get_child(exp->node, ENC_CONDITION);
    struct ExpandoNode *node_true = node_get_child(exp->node, ENC_TRUE);
    struct ExpandoNode *node_false = node_get_child(exp->node, ENC_FALSE);

    check_node_condbool(node_cond, "a");

    struct ExpandoNode *t = node_true;
    check_node_cond(t);

    struct ExpandoNode *f = node_false;
    check_node_cond(f);

    node_cond = node_get_child(t, ENC_CONDITION);
    node_true = node_get_child(t, ENC_TRUE);
    node_false = node_get_child(t, ENC_FALSE);

    check_node_condbool(node_cond, "b");
    check_node_expando(node_true, "c", NULL);
    check_node_expando(node_false, "d", NULL);

    node_cond = node_get_child(f, ENC_CONDITION);
    node_true = node_get_child(f, ENC_TRUE);
    node_false = node_get_child(f, ENC_FALSE);

    check_node_condbool(node_cond, "e");
    check_node_expando(node_true, "f", NULL);
    check_node_expando(node_false, "g", NULL);

    expando_free(&exp);
  }

  {
    const char *input = "%<a?%<b?%c&%d>&%<e?%f>>";

    struct Expando *exp = expando_parse(input, TestFormatDef, err);
    TEST_CHECK(exp != NULL);
    TEST_CHECK(buf_is_empty(err));

    struct ExpandoNode *node_cond = node_get_child(exp->node, ENC_CONDITION);
    struct ExpandoNode *node_true = node_get_child(exp->node, ENC_TRUE);
    struct ExpandoNode *node_false = node_get_child(exp->node, ENC_FALSE);

    check_node_condbool(node_cond, "a");

    struct ExpandoNode *t = node_true;
    check_node_cond(t);

    struct ExpandoNode *f = node_false;
    check_node_cond(f);

    node_cond = node_get_child(t, ENC_CONDITION);
    node_true = node_get_child(t, ENC_TRUE);
    node_false = node_get_child(t, ENC_FALSE);

    check_node_condbool(node_cond, "b");
    check_node_expando(node_true, "c", NULL);
    check_node_expando(node_false, "d", NULL);

    node_cond = node_get_child(f, ENC_CONDITION);
    node_true = node_get_child(f, ENC_TRUE);
    node_false = node_get_child(f, ENC_FALSE);

    check_node_condbool(node_cond, "e");
    check_node_expando(node_true, "f", NULL);
    TEST_CHECK(node_false == NULL);

    expando_free(&exp);
  }

  {
    const char *input = "%<a?%<b?%c&%d>&%<e?&%f>>";

    struct Expando *exp = expando_parse(input, TestFormatDef, err);
    TEST_CHECK(exp != NULL);
    TEST_CHECK(buf_is_empty(err));

    struct ExpandoNode *node_cond = node_get_child(exp->node, ENC_CONDITION);
    struct ExpandoNode *node_true = node_get_child(exp->node, ENC_TRUE);
    struct ExpandoNode *node_false = node_get_child(exp->node, ENC_FALSE);

    check_node_condbool(node_cond, "a");

    struct ExpandoNode *t = node_true;
    check_node_cond(t);

    struct ExpandoNode *f = node_false;
    check_node_cond(f);

    node_cond = node_get_child(t, ENC_CONDITION);
    node_true = node_get_child(t, ENC_TRUE);
    node_false = node_get_child(t, ENC_FALSE);

    check_node_condbool(node_cond, "b");
    check_node_expando(node_true, "c", NULL);
    check_node_expando(node_false, "d", NULL);

    node_cond = node_get_child(f, ENC_CONDITION);
    node_true = node_get_child(f, ENC_TRUE);
    node_false = node_get_child(f, ENC_FALSE);

    check_node_condbool(node_cond, "e");
    TEST_CHECK(node_true == NULL);
    check_node_expando(node_false, "f", NULL);

    expando_free(&exp);
  }

  {
    const char *input = "%<a?%<b?%c>&%<e?%f&%g>>";

    struct Expando *exp = expando_parse(input, TestFormatDef, err);
    TEST_CHECK(exp != NULL);
    TEST_CHECK(buf_is_empty(err));

    struct ExpandoNode *node_cond = node_get_child(exp->node, ENC_CONDITION);
    struct ExpandoNode *node_true = node_get_child(exp->node, ENC_TRUE);
    struct ExpandoNode *node_false = node_get_child(exp->node, ENC_FALSE);

    check_node_condbool(node_cond, "a");

    struct ExpandoNode *t = node_true;
    check_node_cond(t);

    struct ExpandoNode *f = node_false;
    check_node_cond(f);

    node_cond = node_get_child(t, ENC_CONDITION);
    node_true = node_get_child(t, ENC_TRUE);
    node_false = node_get_child(t, ENC_FALSE);

    check_node_condbool(node_cond, "b");
    check_node_expando(node_true, "c", NULL);
    TEST_CHECK(node_false == NULL);

    node_cond = node_get_child(f, ENC_CONDITION);
    node_true = node_get_child(f, ENC_TRUE);
    node_false = node_get_child(f, ENC_FALSE);

    check_node_condbool(node_cond, "e");
    check_node_expando(node_true, "f", NULL);
    check_node_expando(node_false, "g", NULL);

    expando_free(&exp);
  }

  {
    const char *input = "%<a?%<b?&%c>&%<e?%f&%g>>";

    struct Expando *exp = expando_parse(input, TestFormatDef, err);
    TEST_CHECK(exp != NULL);
    TEST_CHECK(buf_is_empty(err));

    struct ExpandoNode *node_cond = node_get_child(exp->node, ENC_CONDITION);
    struct ExpandoNode *node_true = node_get_child(exp->node, ENC_TRUE);
    struct ExpandoNode *node_false = node_get_child(exp->node, ENC_FALSE);

    check_node_condbool(node_cond, "a");

    struct ExpandoNode *t = node_true;
    check_node_cond(t);

    struct ExpandoNode *f = node_false;
    check_node_cond(f);

    node_cond = node_get_child(t, ENC_CONDITION);
    node_true = node_get_child(t, ENC_TRUE);
    node_false = node_get_child(t, ENC_FALSE);

    check_node_condbool(node_cond, "b");
    TEST_CHECK(node_true == NULL);
    check_node_expando(node_false, "c", NULL);

    node_cond = node_get_child(f, ENC_CONDITION);
    node_true = node_get_child(f, ENC_TRUE);
    node_false = node_get_child(f, ENC_FALSE);

    check_node_condbool(node_cond, "e");
    check_node_expando(node_true, "f", NULL);
    check_node_expando(node_false, "g", NULL);

    expando_free(&exp);
  }

  buf_pool_release(&err);
}
