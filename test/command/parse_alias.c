/**
 * @file
 * Test code for parse_alias()
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
#include "config/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "common.h"
#include "test_common.h"

static const struct Command Alias = { "alias", CMD_ALIAS, NULL, CMD_NO_DATA };

static struct ConfigDef Vars[] = {
  // clang-format off
  { "idn_decode", DT_BOOL, true, 0, NULL, },
  { "idn_encode", DT_BOOL, true, 0, NULL, },
  { NULL },
  // clang-format on
};

// clang-format off
static const struct CommandTest Tests[] = {
  // alias [ -group name ...] key address [ , address ...] [ # [ comments ] [ tags:... ]]
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_ERROR,   "js" },
  { MUTT_CMD_SUCCESS, "js1 'John Smith <js@example.com>'" },
  { MUTT_CMD_SUCCESS, "js2 'John Smith <js@example.com>' # comments" },
  { MUTT_CMD_SUCCESS, "js3 'John Smith <js@example.com>' # tags:red,blue" },
  { MUTT_CMD_SUCCESS, "js4 'John Smith <js@example.com>' # comments tags:yellow,green more" },
  { MUTT_CMD_SUCCESS, "friends 'John Smith <js@example.com>', 'Mike Jones <mj@example.com>'" },
  { MUTT_CMD_SUCCESS, "work js2, 'Bob Williams <bw@example.com>', js3" },
  { MUTT_CMD_SUCCESS, "other -group misc j1, j4, work" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

void test_parse_alias(void)
{
  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars));

  // enum CommandResult parse_alias(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; Tests[i].line; i++)
  {
    TEST_CASE(Tests[i].line);
    buf_reset(err);
    buf_strcpy(line, Tests[i].line);
    buf_seek(line, 0);
    rc = parse_alias(&Alias, line, err);
    TEST_CHECK_NUM_EQ(rc, Tests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}
