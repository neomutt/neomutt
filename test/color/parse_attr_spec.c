/**
 * @file
 * Test code for Colour Parsing
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
#include "core/lib.h"
#include "gui/lib.h"
#include "color/lib.h"
#include "test_common.h"

struct AttrTest
{
  const char *str;
  int value;
};

void test_parse_attr_spec(void)
{
  // enum CommandResult parse_attr_spec (struct Buffer *token, struct Buffer *line, struct AttrColor *ac, struct Buffer *err);

  {
    struct Buffer *token = buf_pool_get();
    struct Buffer *line = buf_pool_get();
    struct Buffer *err = buf_pool_get();
    enum CommandResult rc;

    struct AttrColor *ac = attr_color_new();

    buf_addstr(line, "underline");
    buf_seek(line, 0);

    rc = parse_attr_spec(NULL, line, ac, err);
    TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);

    rc = parse_attr_spec(token, NULL, ac, err);
    TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);

    rc = parse_attr_spec(token, line, NULL, err);
    TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);

    attr_color_free(&ac);

    buf_pool_release(&token);
    buf_pool_release(&line);
    buf_pool_release(&err);
  }

  {
    struct Buffer *token = buf_pool_get();
    struct Buffer *line = buf_pool_get();
    struct Buffer *err = buf_pool_get();

    struct AttrColor *ac = attr_color_new();

    struct AttrTest tests[] = {
      // clang-format off
      { "bold",      A_BOLD      },
      { "italic",    A_ITALIC    },
      { "NONE",      A_NORMAL    },
      { "normal",    A_NORMAL    },
      { "REVERSE",   A_REVERSE   },
      { "standout",  A_STANDOUT  },
      { "UnDeRlInE", A_UNDERLINE },
      { NULL, 0 },
      // clang-format on
    };

    for (int i = 0; tests[i].str; i++)
    {
      buf_strcpy(line, tests[i].str);
      buf_seek(line, 0);

      enum CommandResult rc = parse_attr_spec(token, line, ac, err);
      TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
      TEST_MSG("err: %line", buf_string(err));
    }

    attr_color_free(&ac);

    buf_pool_release(&token);
    buf_pool_release(&line);
    buf_pool_release(&err);
  }

  {
    struct Buffer *token = buf_pool_get();
    struct Buffer *line = buf_pool_get();
    struct Buffer *err = buf_pool_get();

    struct AttrColor *ac = attr_color_new();

    const char *tests[] = { "", "reversed", NULL };

    for (int i = 0; tests[i]; i++)
    {
      buf_addstr(line, tests[i]);
      buf_seek(line, 0);

      enum CommandResult rc = parse_attr_spec(token, line, ac, err);
      TEST_CHECK_NUM_EQ(rc, MUTT_CMD_WARNING);
    }

    attr_color_free(&ac);

    buf_pool_release(&token);
    buf_pool_release(&line);
    buf_pool_release(&err);
  }
}
