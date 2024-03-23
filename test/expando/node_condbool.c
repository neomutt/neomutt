/**
 * @file
 * Test CondBool Expando
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
#include <time.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

struct ExpandoNode *node_condbool_new(const char *start, const char *end, int did, int uid);
struct ExpandoNode *node_conddate_new(int count, char period, int did, int uid);

static long test_y_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  return 42;
}

static void test_y(const struct ExpandoNode *node, void *data,
                   MuttFormatFlags flags, int max_cols, struct Buffer *buf)
{
  buf_strcpy(buf, "hello");
}

static long test_n_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  return 0;
}

static void test_n(const struct ExpandoNode *node, void *data,
                   MuttFormatFlags flags, int max_cols, struct Buffer *buf)
{
}

static struct ExpandoNode *parse_test(const char *str, const char **parsed_until,
                                      int did, int uid, ExpandoParserFlags flags,
                                      struct ExpandoParseError *error)
{
  return node_condbool_new(str, str + 1, did, uid);
}

void test_expando_node_condbool(void)
{
  static const struct ExpandoDefinition TestFormatDef[] = {
    // clang-format off
    { "a",  "apple",  1, 2, E_TYPE_STRING, NULL },
    { "b",  "banana", 1, 3, E_TYPE_STRING, NULL },
    { "c",  "cherry", 1, 4, E_TYPE_STRING, parse_test },
    { "d",  "damson", 1, 5, E_TYPE_STRING, parse_test },
    { "e",  "endive", 1, 6, E_TYPE_STRING, NULL },
    { NULL, NULL, 0, -1, -1, NULL }
    // clang-format on
  };

  static const struct ExpandoRenderData TestRenderData[] = {
    // clang-format off
    { 1, 2, test_y, test_y_num },
    { 1, 3, test_n, test_n_num },
    { 1, 4, test_y, NULL },
    { 1, 5, test_n, NULL },
    { 1, 6, NULL,   NULL },
    { -1, -1, NULL, NULL },
    // clang-format on
  };

  // struct ExpandoNode *node_condbool_new(const char *start, const char *end, int did, int uid);
  {
    const char *str = "%x";

    struct ExpandoNode *node = node_condbool_new(str + 1, str + 2, 1, 2);
    TEST_CHECK(node != NULL);
    node_free(&node);
  }

  // struct ExpandoNode *node_condbool_parse(const char *s, const char **parsed_until, const struct ExpandoDefinition *defs, ExpandoParserFlags flags, struct ExpandoParseError *error);
  {
    const char *str = "%a";
    const char *parsed_until = NULL;
    struct ExpandoParseError err = { 0 };

    struct ExpandoNode *node = node_condbool_parse(str, &parsed_until, TestFormatDef,
                                                   EP_NO_FLAGS, &err);
    TEST_CHECK(node != NULL);
    node_free(&node);

    str = "%c";
    node = node_condbool_parse(str, &parsed_until, TestFormatDef, EP_NO_FLAGS, &err);
    TEST_CHECK(node != NULL);
    node_free(&node);

    str = "%Q";
    node = node_condbool_parse(str, &parsed_until, TestFormatDef, EP_NO_FLAGS, &err);
    TEST_CHECK(node == NULL);
  }

  // int node_condbool_render(const struct ExpandoNode *node, const struct ExpandoRenderData *rdata, struct Buffer *buf, int max_cols, void *data, MuttFormatFlags flags);
  {
    struct Buffer *buf = buf_pool_get();
    struct ExpandoNode *node = NULL;
    const char *str = NULL;
    int rc;

    const char *parsed_until = NULL;
    struct ExpandoParseError err = { 0 };

    str = "%a";
    node = node_condbool_parse(str, &parsed_until, TestFormatDef, EP_NO_FLAGS, &err);
    TEST_CHECK(node != NULL);
    rc = node_condbool_render(node, TestRenderData, buf, 99, NULL, MUTT_FORMAT_NO_FLAGS);
    TEST_CHECK(rc == 1);
    node_free(&node);

    str = "%b";
    node = node_condbool_parse(str, &parsed_until, TestFormatDef, EP_NO_FLAGS, &err);
    TEST_CHECK(node != NULL);
    rc = node_condbool_render(node, TestRenderData, buf, 99, NULL, MUTT_FORMAT_NO_FLAGS);
    TEST_CHECK(rc == 0);
    node_free(&node);

    str = "%c";
    node = node_condbool_parse(str, &parsed_until, TestFormatDef, EP_NO_FLAGS, &err);
    TEST_CHECK(node != NULL);
    rc = node_condbool_render(node, TestRenderData, buf, 99, NULL, MUTT_FORMAT_NO_FLAGS);
    TEST_CHECK(rc == 1);
    node_free(&node);

    str = "%d";
    node = node_condbool_parse(str, &parsed_until, TestFormatDef, EP_NO_FLAGS, &err);
    TEST_CHECK(node != NULL);
    rc = node_condbool_render(node, TestRenderData, buf, 99, NULL, MUTT_FORMAT_NO_FLAGS);
    TEST_CHECK(rc == 0);
    node_free(&node);

    str = "%e";
    node = node_condbool_parse(str, &parsed_until, TestFormatDef, EP_NO_FLAGS, &err);
    TEST_CHECK(node != NULL);
    rc = node_condbool_render(node, TestRenderData, buf, 99, NULL, MUTT_FORMAT_NO_FLAGS);
    TEST_CHECK(rc == 0);
    node_free(&node);

    buf_pool_release(&buf);
  }
}
