/**
 * @file
 * Test code for node_condition_parse()
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
#include <string.h>
#include "email/lib.h"
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep

static struct ExpandoNode *parse_test1(const char *str, struct ExpandoFormat *fmt,
                                       int did, int uid, ExpandoParserFlags flags,
                                       const char **parsed_until,
                                       struct ExpandoParseError *err)
{
  *parsed_until = str + 1;
  return node_expando_new(fmt, did, uid);
}

void test_expando_node_condition_parse(void)
{
  // struct ExpandoNode *node_condition_parse(const char *str, NodeTextTermFlags term_chars, const struct ExpandoDefinition *defs, const char **parsed_until, struct ExpandoParseError *err);

  static const struct ExpandoDefinition TestFormatDef[] = {
    // clang-format off
    { "a", NULL, ED_EMAIL,    ED_EMA_STRF_RECV_LOCAL, parse_test1 },
    { "b", NULL, ED_EMAIL,    ED_EMA_INDEX_HOOK,      parse_test1 },
    { "c", NULL, ED_ENVELOPE, ED_ENV_FROM,            NULL        },
    { "d", NULL, ED_ENVELOPE, ED_ENV_SENDER,          NULL        },
    { NULL, NULL, 0, -1, NULL }
    // clang-format on
  };

  // Degenerate
  {
    const char *parsed_until = NULL;
    struct ExpandoNode *node = NULL;
    struct ExpandoParseError err = { 0 };

    node = node_condition_parse(NULL, NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node == NULL);

    node = node_condition_parse("", NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node == NULL);

    node = node_condition_parse("abc", NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node == NULL);

    node = node_condition_parse("%abc", NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node == NULL);
  }

  {
    const char *parsed_until = NULL;
    struct ExpandoNode *node = NULL;
    struct ExpandoParseError err = { 0 };
    const char *str = NULL;

    str = "%9999999<"; // Bad format
    node = node_condition_parse(str, NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node == NULL);
    memset(&err, 0, sizeof(err));

    str = "%x"; // Not a conditional
    node = node_condition_parse(str, NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node == NULL);
    memset(&err, 0, sizeof(err));

    str = "%<x?"; // Not a valid expando
    node = node_condition_parse(str, NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node == NULL);
    memset(&err, 0, sizeof(err));

    str = "%<a?"; // Missing true, false, end
    node = node_condition_parse(str, NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node == NULL);
    memset(&err, 0, sizeof(err));

    str = "%<a?aaa"; // Missing false, end
    node = node_condition_parse(str, NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node == NULL);
    memset(&err, 0, sizeof(err));

    str = "%<a?%-99999b"; // Bad true
    node = node_condition_parse(str, NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node == NULL);
    memset(&err, 0, sizeof(err));

    str = "%<a?aaa&"; // Missing false, end
    node = node_condition_parse(str, NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node == NULL);
    memset(&err, 0, sizeof(err));

    str = "%<a?aaa&%-99999b"; // Bad false
    node = node_condition_parse(str, NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node == NULL);
    memset(&err, 0, sizeof(err));

    str = "%<a?aaa&bbb"; // Missing end
    node = node_condition_parse(str, NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node == NULL);
    memset(&err, 0, sizeof(err));

    str = "%<a?aaa&bbb>"; // Valid conditional
    node = node_condition_parse(str, NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node != NULL);
    node_free(&node);
  }
}
