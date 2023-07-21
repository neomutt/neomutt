/**
 * @file
 * Test Expando helpers
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
#include "mutt/lib.h"
#include "email/lib.h"
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep

static void index_a(const struct ExpandoNode *node, void *data,
                    MuttFormatFlags flags, int max_cols, struct Buffer *buf)
{
  buf_strcpy(buf, "apple");
}

static long index_a_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  return 42;
}

void test_expando_helpers(void)
{
  static const struct ExpandoDefinition TestFormatDef[] = {
    // clang-format off
    { "a",  "from", ED_ENVELOPE, ED_ENV_FROM,      E_TYPE_STRING, NULL },
    { "xy", "from", ED_ENVELOPE, ED_ENV_FROM_FULL, E_TYPE_STRING, NULL },
    { NULL, NULL, 0, -1, -1, NULL }
    // clang-format on
  };

  static const struct ExpandoRenderData TestRenderData[] = {
    // clang-format off
    { 1, 2, index_a, index_a_num },
    { -1, -1, NULL, NULL },
    // clang-format on
  };

  // const struct ExpandoRenderData *find_get_number(const struct ExpandoRenderData *rdata, int did, int uid);
  {
    const struct ExpandoRenderData *rdata = NULL;

    rdata = find_get_number(NULL, 1, 2);
    TEST_CHECK(rdata == NULL);

    rdata = find_get_number(TestRenderData, 1, 2);
    TEST_CHECK(rdata != NULL);

    rdata = find_get_number(TestRenderData, 10, 20);
    TEST_CHECK(rdata == NULL);
  }

  // const struct ExpandoRenderData *find_get_string(const struct ExpandoRenderData *rdata, int did, int uid);
  {
    const struct ExpandoRenderData *rdata = NULL;

    rdata = find_get_string(NULL, 1, 2);
    TEST_CHECK(rdata == NULL);

    rdata = find_get_string(TestRenderData, 1, 2);
    TEST_CHECK(rdata != NULL);

    rdata = find_get_string(TestRenderData, 10, 20);
    TEST_CHECK(rdata == NULL);
  }

  // const char *skip_until_ch(const char *start, char terminator);
  {
    const char *str = "appleX";
    const char *end = NULL;

    end = skip_until_ch("", 'X');
    TEST_CHECK(end != NULL);
    TEST_CHECK(*end == '\0');

    end = skip_until_ch(str, 'X');
    TEST_CHECK(end != NULL);
    TEST_CHECK(*end == 'X');
  }

  // const char *skip_until_classic_expando(const char *start);
  {
    const char *str = "%q apple";
    const char *end = NULL;

    end = skip_until_classic_expando("");
    TEST_CHECK(end != NULL);
    TEST_CHECK(*end == '\0');

    end = skip_until_classic_expando(str);
    TEST_CHECK(end != NULL);
    TEST_CHECK(*end == 'q');
  }

  // const char *skip_classic_expando(const char *str, const struct ExpandoDefinition *defs);
  {
    const char *str = "%aXapple";
    const char *end = NULL;

    end = skip_classic_expando(str + 1, TestFormatDef);
    TEST_CHECK(end != NULL);
    TEST_CHECK(*end == 'X');

    str = "%xyQapple";

    end = skip_classic_expando(str + 1, TestFormatDef);
    TEST_CHECK(end != NULL);
    TEST_CHECK(*end == 'Q');

    str = "%Qapple";

    end = skip_classic_expando(str + 1, TestFormatDef);
    TEST_CHECK(end != NULL);
    TEST_CHECK(*end == 'a');
  }
}
