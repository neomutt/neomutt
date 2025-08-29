/**
 * @file
 * Test the Expando Parser
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
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "alias/gui.h" // IWYU pragma: keep
#include "alias/lib.h"
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

void expando_serialise(const struct Expando *exp, struct Buffer *buf);

void test_expando_parser(void)
{
  static const struct ExpandoDefinition TestFormatDef[] = {
    // clang-format off
    { "*", "padding-soft",     ED_GLOBAL, ED_GLO_PADDING_SOFT,     node_padding_parse },
    { ">", "padding-hard",     ED_GLOBAL, ED_GLO_PADDING_HARD,     node_padding_parse },
    { "|", "padding-eol",      ED_GLOBAL, ED_GLO_PADDING_EOL,      node_padding_parse },
    { "X", "attachment-count", ED_EMAIL,  ED_EMA_ATTACHMENT_COUNT, NULL },
    { "[", NULL,               ED_EMAIL,  ED_EMA_STRF_LOCAL,       parse_date },
    { "a", "apple",            ED_ALIAS,  ED_ALI_ADDRESS,          NULL },
    { "b", "banana",           ED_ALIAS,  ED_ALI_COMMENT,          NULL },
    { "c", "cherry",           ED_ALIAS,  ED_ALI_FLAGS,            NULL },
    { "d", "damson",           ED_ALIAS,  ED_ALI_NAME,             NULL },
    { NULL, NULL, 0, -1, NULL },
    // clang-format on
  };

  static const char *TestStrings[][2] = {
    // clang-format off
    // Formatting
    { "",       "" },
    { "%X",     "<EXP:(EMAIL,ATTACHMENT_COUNT)>" },
    { "%5X",    "<EXP:(EMAIL,ATTACHMENT_COUNT):{5,-1,RIGHT,' '}>" },
    { "%.7X",   "<EXP:(EMAIL,ATTACHMENT_COUNT):{0,7,RIGHT,'0'}>" },
    { "%5.7X",  "<EXP:(EMAIL,ATTACHMENT_COUNT):{5,7,RIGHT,'0'}>" },
    { "%-5X",   "<EXP:(EMAIL,ATTACHMENT_COUNT):{5,-1,LEFT,' '}>" },
    { "%-.7X",  "<EXP:(EMAIL,ATTACHMENT_COUNT):{0,7,LEFT,'0'}>" },
    { "%-5.7X", "<EXP:(EMAIL,ATTACHMENT_COUNT):{5,7,LEFT,'0'}>" },
    { "%05X",   "<EXP:(EMAIL,ATTACHMENT_COUNT):{5,-1,RIGHT,'0'}>" },

    // Conditional (old form)
    { "%?X??",           "<COND:<BOOL(EMAIL,ATTACHMENT_COUNT)>||>" },
    { "%?X?&?",          "<COND:<BOOL(EMAIL,ATTACHMENT_COUNT)>||>" },
    { "%?X?AAA?",        "<COND:<BOOL(EMAIL,ATTACHMENT_COUNT)>|<TEXT:'AAA'>|>" },
    { "%?X?AAA&?",       "<COND:<BOOL(EMAIL,ATTACHMENT_COUNT)>|<TEXT:'AAA'>|>" },
    { "%?X?&BBB?",       "<COND:<BOOL(EMAIL,ATTACHMENT_COUNT)>||<TEXT:'BBB'>>" },
    { "%?X?AAA&BBB?",    "<COND:<BOOL(EMAIL,ATTACHMENT_COUNT)>|<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "%=30?X?AAA&BBB?", "<COND:<BOOL(EMAIL,ATTACHMENT_COUNT)>|<TEXT:'AAA'>|<TEXT:'BBB'>:{30,-1,CENTER,' '}>" },

    // Conditional (new form)
    { "%<X?>",           "<COND:<BOOL(EMAIL,ATTACHMENT_COUNT)>||>" },
    { "%<X?&>",          "<COND:<BOOL(EMAIL,ATTACHMENT_COUNT)>||>" },
    { "%<X?AAA>",        "<COND:<BOOL(EMAIL,ATTACHMENT_COUNT)>|<TEXT:'AAA'>|>" },
    { "%<X?AAA&>",       "<COND:<BOOL(EMAIL,ATTACHMENT_COUNT)>|<TEXT:'AAA'>|>" },
    { "%<X?&BBB>",       "<COND:<BOOL(EMAIL,ATTACHMENT_COUNT)>||<TEXT:'BBB'>>" },
    { "%<X?AAA&BBB>",    "<COND:<BOOL(EMAIL,ATTACHMENT_COUNT)>|<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "%=30<X?AAA&BBB>", "<COND:<BOOL(EMAIL,ATTACHMENT_COUNT)>|<TEXT:'AAA'>|<TEXT:'BBB'>:{30,-1,CENTER,' '}>" },

    // Dates
    { "%[%Y-%m-%d]",    "<EXP:'%Y-%m-%d'(EMAIL,STRF_LOCAL)>" },
    { "%-5[%Y-%m-%d]",  "<EXP:'%Y-%m-%d'(EMAIL,STRF_LOCAL):{5,-1,LEFT,' '}>" },

    // Conditional dates
    { "%<[1M?AAA&BBB>",  "<COND:<DATE:(EMAIL,STRF_LOCAL):1:M>|<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "%<[10M?AAA&BBB>", "<COND:<DATE:(EMAIL,STRF_LOCAL):10:M>|<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "%<[1H?AAA&BBB>",  "<COND:<DATE:(EMAIL,STRF_LOCAL):1:H>|<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "%<[10H?AAA&BBB>", "<COND:<DATE:(EMAIL,STRF_LOCAL):10:H>|<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "%<[1d?AAA&BBB>",  "<COND:<DATE:(EMAIL,STRF_LOCAL):1:d>|<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "%<[10d?AAA&BBB>", "<COND:<DATE:(EMAIL,STRF_LOCAL):10:d>|<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "%<[1w?AAA&BBB>",  "<COND:<DATE:(EMAIL,STRF_LOCAL):1:w>|<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "%<[10w?AAA&BBB>", "<COND:<DATE:(EMAIL,STRF_LOCAL):10:w>|<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "%<[1m?AAA&BBB>",  "<COND:<DATE:(EMAIL,STRF_LOCAL):1:m>|<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "%<[10m?AAA&BBB>", "<COND:<DATE:(EMAIL,STRF_LOCAL):10:m>|<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "%<[1y?AAA&BBB>",  "<COND:<DATE:(EMAIL,STRF_LOCAL):1:y>|<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "%<[10y?AAA&BBB>", "<COND:<DATE:(EMAIL,STRF_LOCAL):10:y>|<TEXT:'AAA'>|<TEXT:'BBB'>>" },

    // Padding
    { "AAA%>XBBB", "<PAD:HARD_FILL:'X':<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "AAA%|XBBB", "<PAD:FILL_EOL:'X':<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    { "AAA%*XBBB", "<PAD:SOFT_FILL:'X':<TEXT:'AAA'>|<TEXT:'BBB'>>" },
    // clang-format on
  };

  {
    struct Buffer *buf = buf_pool_get();
    struct Buffer *err = buf_pool_get();
    struct Expando *exp = NULL;

    for (int i = 0; i < mutt_array_size(TestStrings); i++)
    {
      buf_reset(buf);
      buf_reset(err);

      const char *format = TestStrings[i][0];
      const char *expected = TestStrings[i][1];
      TEST_CASE(format);

      exp = expando_parse(format, TestFormatDef, err);
      TEST_CHECK(buf_is_empty(err));
      TEST_MSG(buf_string(err));
      expando_serialise(exp, buf);
      TEST_CHECK_STR_EQ(buf_string(buf), expected);
      expando_free(&exp);
    }

    buf_pool_release(&buf);
    buf_pool_release(&err);
  }

  {
    static const char *TestsBad[] = {
      // clang-format off
      "%<a?%Q&bbb>",
      "%<a?aaa&%Q>",
      "%<Q?aaa&bbb>",
      "%<[99999b?aaa&bbb>",
      "%<[a?aaa&bbb>",
      "%99999c",
      "%4.99999c",
      "%Q",
      "%[%a",
      "%<*?aaa&bbb>",
      "%<baaa&bbb>",
      "%<b?aaa",
      "%<b?aaa&bbb",
      // clang-format off
    };

    struct Buffer *buf = buf_pool_get();
    struct Buffer *err = buf_pool_get();
    struct Expando *exp = NULL;

    for (int i = 0; i < mutt_array_size(TestsBad); i++)
    {
      buf_reset(buf);
      buf_reset(err);

      const char *format = TestsBad[i];
      TEST_CASE(format);

      exp = expando_parse(format, TestFormatDef, err);
      TEST_CHECK(exp == NULL);
      TEST_CHECK(!buf_is_empty(err));
      expando_free(&exp);
    }

    buf_pool_release(&buf);
    buf_pool_release(&err);
  }

  {
    struct ExpandoNode *node = NULL;
    const char *parsed_until = NULL;
    struct ExpandoParseError err = { 0 };

    // struct ExpandoNode *node_parse_one(const char *str, NodeTextTermFlags term_chars, const struct ExpandoDefinition *defs, const char **parsed_until, struct ExpandoParseError *err);

    node = node_parse_one(NULL, NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node == NULL);

    node = node_parse_one("", NTE_NO_FLAGS, NULL, &parsed_until, &err);
    TEST_CHECK(node == NULL);

    node = node_parse_one("", NTE_NO_FLAGS, TestFormatDef, NULL, &err);
    TEST_CHECK(node == NULL);

    node = node_parse_one("", NTE_NO_FLAGS, TestFormatDef, &parsed_until, NULL);
    TEST_CHECK(node == NULL);

    node = node_parse_one("abc", NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node != NULL);
    node_free(&node);

    node = node_parse_one("%<a?b&c>", NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node != NULL);
    node_free(&node);

    node = node_parse_one("%a", NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(node != NULL);
    node_free(&node);
  }

  {
    struct ExpandoNode *node = node_new();
    const char *parsed_until = NULL;
    struct ExpandoParseError err = { 0 };
    bool rc = false;

    rc = node_parse_many(NULL, "%a", NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(rc == false);

    rc = node_parse_many(node, NULL, NTE_NO_FLAGS, TestFormatDef, &parsed_until, &err);
    TEST_CHECK(rc == false);

    rc = node_parse_many(node, "%a", NTE_NO_FLAGS, NULL, &parsed_until, &err);
    TEST_CHECK(rc == false);

    rc = node_parse_many(node, "%a", NTE_NO_FLAGS, TestFormatDef, NULL, &err);
    TEST_CHECK(rc == false);

    rc = node_parse_many(node, "%a", NTE_NO_FLAGS, TestFormatDef, &parsed_until, NULL);
    TEST_CHECK(rc == false);

    node_free(&node);
  }
}
