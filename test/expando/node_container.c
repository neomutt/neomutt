/**
 * @file
 * Test Container Expando
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
#include <string.h>
#include "mutt/lib.h"
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

struct ExpandoFormat *parse_format(const char *start, const char *end,
                                   struct ExpandoParseError *error);

void test_one(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  buf_addstr(buf, "ONE");
  buf_addch(buf, 'a' - 1 + node->uid);
  buf_addch(buf, 'a' - 1 + node->uid);
  buf_addch(buf, 'a' - 1 + node->uid);
}

void test_two(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  buf_addstr(buf, "TWO");
  buf_addch(buf, 'a' - 1 + node->uid);
  buf_addch(buf, 'a' - 1 + node->uid);
  buf_addch(buf, 'a' - 1 + node->uid);
}

void test_three(const struct ExpandoNode *node, void *data,
                MuttFormatFlags flags, int max_cols, struct Buffer *buf)
{
  buf_addstr(buf, "THREE");
  buf_addch(buf, 'a' - 1 + node->uid);
  buf_addch(buf, 'a' - 1 + node->uid);
  buf_addch(buf, 'a' - 1 + node->uid);
}

struct ExpandoNode *make_children(char ch)
{
  struct ExpandoNode *n1 = node_expando_new(NULL, NULL, NULL, (ch - 'a' + 1) * 10, 1);
  struct ExpandoNode *n2 = node_expando_new(NULL, NULL, NULL, (ch - 'a' + 1) * 10, 2);
  struct ExpandoNode *n3 = node_expando_new(NULL, NULL, NULL, (ch - 'a' + 1) * 10, 3);

  n1->next = n2;
  n2->next = n3;

  return n1;
}

void test_expando_node_container(void)
{
  const struct ExpandoRenderData TestRenderData[] = {
    // clang-format off
    { 10, 1, test_one, NULL },
    { 10, 2, test_one, NULL },
    { 10, 3, test_one, NULL },
    { 20, 1, test_two, NULL },
    { 20, 2, test_two, NULL },
    { 20, 3, test_two, NULL },
    { 30, 1, test_three, NULL },
    { 30, 2, test_three, NULL },
    { 30, 3, test_three, NULL },
    { -1, -1, NULL, NULL },
    // clang-format on
  };

  // struct ExpandoNode *node_container_new(void);
  {
    struct ExpandoNode *node = node_container_new();
    TEST_CHECK(node != NULL);
    node_free(&node);
  }

  // int node_container_render(const struct ExpandoNode *node, const struct ExpandoRenderData *rdata, struct Buffer *buf, int max_cols, void *data, MuttFormatFlags flags);
  {
    static const struct Mapping tests[] = {
      // clang-format off
      { "ONEaaaONEbbbONEcccTW", 25 },
      { "ONEaaaONEbbbONE", 15 },
      { "ONEaaaONEb", 10 },
      { "", 0 },
      // clang-format on
    };

    struct ExpandoParseError err = { 0 };
    const char *fmt_str = "-15.20x";
    const char *fmt_end = strchr(fmt_str, 'x');
    struct ExpandoNode *cont = node_container_new();
    cont->format = parse_format(fmt_str, fmt_end, &err);

    node_set_child(cont, 0, make_children('a'));
    node_set_child(cont, 1, make_children('b'));
    node_set_child(cont, 2, make_children('c'));

    struct Buffer *buf = buf_pool_get();
    int rc;

    for (size_t i = 0; i < mutt_array_size(tests); i++)
    {
      TEST_CASE_("%d", tests[i].value);
      buf_reset(buf);
      rc = node_tree_render(cont, TestRenderData, buf, tests[i].value, NULL, MUTT_FORMAT_NO_FLAGS);
      TEST_CHECK(rc == strlen(tests[i].name));
      TEST_CHECK_STR_EQ(buf_string(buf), tests[i].name);
    }

    FREE(&cont->format);
    buf_reset(buf);
    rc = node_tree_render(cont, TestRenderData, buf, 50, NULL, MUTT_FORMAT_NO_FLAGS);
    TEST_CHECK(rc == 50);
    TEST_CHECK_STR_EQ(buf_string(buf), "ONEaaaONEbbbONEcccTWOaaaTWObbbTWOcccTHREEaaaTHREEb");

    fmt_str = "_-15.20x";
    fmt_end = strchr(fmt_str, 'x');
    cont->format = parse_format(fmt_str, fmt_end, &err);
    buf_reset(buf);
    rc = node_tree_render(cont, TestRenderData, buf, 20, NULL, MUTT_FORMAT_NO_FLAGS);
    TEST_CHECK(rc == 20);
    TEST_CHECK_STR_EQ(buf_string(buf), "oneaaaonebbboneccctw");

    FREE(&cont->format);

    node_free(&cont);
    buf_pool_release(&buf);
  }
}
