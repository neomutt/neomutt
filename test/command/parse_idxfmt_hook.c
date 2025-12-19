/**
 * @file
 * Test code for parse_idxfmt_hook()
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
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "common.h"
#include "hook.h"
#include "test_common.h"

static const struct Command IndexFormatHook = { "index-format-hook", NULL, MUTT_IDXFMTHOOK };

static struct ConfigDef Vars[] = {
  // clang-format off
  // index-format-hook <name> [ ! ]<pattern> <format-string>
  { "default_hook", DT_STRING, IP "~f %s !~P | (~P ~C %s)", 0, NULL, },
  { NULL },
  // clang-format on
};

// clang-format off
static const struct CommandTest Tests[] = {
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "date '~d<1d' '%[%H:%M]'" },
  { MUTT_CMD_SUCCESS, "date '~d<1m' '%[%a %d]'" },
  { MUTT_CMD_SUCCESS, "date '~d<1y' '%[%b %d]'" },
  { MUTT_CMD_SUCCESS, "date '~A'    '%[%m/%y]'" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

void test_parse_idxfmt_hook(void)
{
  // enum CommandResult parse_idxfmt_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars));

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; Tests[i].line; i++)
  {
    TEST_CASE(Tests[i].line);
    buf_reset(err);
    buf_strcpy(line, Tests[i].line);
    buf_seek(line, 0);
    rc = parse_idxfmt_hook(&IndexFormatHook, line, err);
    TEST_CHECK_NUM_EQ(rc, Tests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}
