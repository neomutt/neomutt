/**
 * @file
 * Test Parse Short Name
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
#include "email/lib.h"
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep

struct TestShortName
{
  const char *expando;
  ExpandoParserFlags flags;
  int did;
  int uid;
};

static struct ExpandoNode *parse_test1(const char *str, struct ExpandoFormat *fmt,
                                       int did, int uid, ExpandoParserFlags flags,
                                       const char **parsed_until,
                                       struct ExpandoParseError *err)
{
  *parsed_until = str + 1;
  return node_expando_new(fmt, did, uid);
}

void test_expando_parse_short_name(void)
{
  // struct ExpandoNode *parse_short_name(const char *str, const struct ExpandoDefinition *defs, ExpandoParserFlags flags, struct ExpandoFormat *fmt, const char **parsed_until, struct ExpandoParseError *err);

  static const struct ExpandoDefinition TestFormatDef[] = {
    // clang-format off
    { "a",  NULL, ED_EMAIL, ED_EMA_STRF_RECV_LOCAL, NULL },
    { "ab", NULL, ED_EMAIL, ED_EMA_THREAD_COUNT,    NULL },
    { "b",  NULL, ED_EMAIL, ED_EMA_INDEX_HOOK,      NULL },
    { "c",  NULL, ED_EMAIL, ED_EMA_DATE_STRF_LOCAL, parse_test1 },
    { "dd", NULL, ED_EMAIL, ED_EMA_DATE_STRF,       parse_test1 },
    { NULL, NULL, 0, -1, NULL }
    // clang-format on
  };

  static const struct TestShortName tests[] = {
    // clang-format off
    { "a",   0,                  ED_EMAIL, ED_EMA_STRF_RECV_LOCAL },
    { "b",   0,                  ED_EMAIL, ED_EMA_INDEX_HOOK      },
    { "bb",  0,                  ED_EMAIL, ED_EMA_INDEX_HOOK      },
    { "c",   EP_NO_CUSTOM_PARSE, ED_EMAIL, ED_EMA_DATE_STRF_LOCAL },
    { "cc",  EP_NO_CUSTOM_PARSE, ED_EMAIL, ED_EMA_DATE_STRF_LOCAL },
    { "dd",  EP_NO_CUSTOM_PARSE, ED_EMAIL, ED_EMA_DATE_STRF       },
    { "dde", EP_NO_CUSTOM_PARSE, ED_EMAIL, ED_EMA_DATE_STRF       },
    { NULL, 0, 0, 0 },
    // clang-format on
  };

  // Degenerate
  {
    struct ExpandoNode *node = NULL;
    const char *parsed_until = NULL;

    node = parse_short_name(NULL, TestFormatDef, EP_NO_FLAGS, NULL, &parsed_until, NULL);
    TEST_CHECK(node == NULL);

    node = parse_short_name("a", NULL, EP_NO_FLAGS, NULL, &parsed_until, NULL);
    TEST_CHECK(node == NULL);
  }

  {
    for (int i = 0; tests[i].expando; i++)
    {
      TEST_CASE(tests[i].expando);

      struct ExpandoNode *node = NULL;
      const char *parsed_until = NULL;

      node = parse_short_name(tests[i].expando, TestFormatDef, tests[i].flags,
                              NULL, &parsed_until, NULL);
      TEST_CHECK(node != NULL);
      TEST_CHECK(node->did == tests[i].did);
      TEST_CHECK(node->uid == tests[i].uid);

      node_free(&node);
    }
  }
}
