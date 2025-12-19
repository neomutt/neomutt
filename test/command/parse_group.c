/**
 * @file
 * Test code for parse_group()
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
#include "address/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "commands.h"
#include "common.h"
#include "test_common.h"

// clang-format off
static const struct Command Group   = { "group",   NULL, MUTT_GROUP };
static const struct Command UnGroup = { "ungroup", NULL, MUTT_UNGROUP };
// clang-format on

static struct ConfigDef Vars[] = {
  // clang-format off
  { "idn_decode", DT_BOOL, true, 0, NULL, },
  { "idn_encode", DT_BOOL, true, 0, NULL, },
  { NULL },
  // clang-format on
};

// clang-format off
static const struct CommandTest GroupTests[] = {
  // group [ -group <name> ... ] { -rx <regex> ... | -addr <address> ... }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "-addr 'Jim Smith <js@example.com>'" },
  { MUTT_CMD_SUCCESS, "-rx '.*@example\\.com'" },
  { MUTT_CMD_SUCCESS, "-group work -addr 'Mike Jones <mj@example.com>'" },
  { MUTT_CMD_SUCCESS, "-group other -rx '.*@example\\.com'" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest UnGroupTests[] = {
  // ungroup [ -group <name> ... ] { * | -rx <regex> ... | -addr <address> ... }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "-addr 'Jim Smith <js@example.com>'" },
  { MUTT_CMD_SUCCESS, "-rx '.*@example\\.com'" },
  { MUTT_CMD_SUCCESS, "-group work -addr 'Mike Jones <mj@example.com>'" },
  { MUTT_CMD_SUCCESS, "-group other -rx '.*@example\\.com'" },
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

static void test_parse_group2(void)
{
  // enum CommandResult parse_group(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; GroupTests[i].line; i++)
  {
    TEST_CASE(GroupTests[i].line);
    buf_reset(err);
    buf_strcpy(line, GroupTests[i].line);
    buf_seek(line, 0);
    rc = parse_group(&Group, line, err);
    TEST_CHECK_NUM_EQ(rc, GroupTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_ungroup(void)
{
  // enum CommandResult parse_group(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; UnGroupTests[i].line; i++)
  {
    TEST_CASE(UnGroupTests[i].line);
    buf_reset(err);
    buf_strcpy(line, UnGroupTests[i].line);
    buf_seek(line, 0);
    rc = parse_group(&UnGroup, line, err);
    TEST_CHECK_NUM_EQ(rc, UnGroupTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

void test_parse_group(void)
{
  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars));

  test_parse_group2();
  test_parse_ungroup();
}
