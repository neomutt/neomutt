/**
 * @file
 * Test code for parse_unbind()
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
#include "core/lib.h"
#include "key/lib.h"
#include "common.h"
#include "test_common.h"

// clang-format off
static const struct Command UnBind  = { "unbind",  NULL, MUTT_UNBIND };
static const struct Command UnMacro = { "unmacro", NULL, MUTT_UNMACRO };
// clang-format on

// clang-format off
static const struct CommandTest UnBindTests[] = {
  // unbind { * | <menu>[,<menu> ... ] } [ <key> ]
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "index j" },
  { MUTT_CMD_SUCCESS, "index,pager s" },
  { MUTT_CMD_SUCCESS, "pager <f1>" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest UnMacroTests[] = {
  // unmacro { * | <menu>[,<menu> ... ] } [ <key> ]
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "index eee" },
  { MUTT_CMD_SUCCESS, "index nn" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

static void test_parse_unbind2(void)
{
  // enum CommandResult parse_unbind(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; UnBindTests[i].line; i++)
  {
    TEST_CASE(UnBindTests[i].line);
    buf_reset(err);
    buf_strcpy(line, UnBindTests[i].line);
    buf_seek(line, 0);
    rc = parse_unbind(&UnBind, line, err);
    TEST_CHECK_NUM_EQ(rc, UnBindTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_unmacro(void)
{
  // enum CommandResult parse_unbind(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; UnMacroTests[i].line; i++)
  {
    TEST_CASE(UnMacroTests[i].line);
    buf_reset(err);
    buf_strcpy(line, UnMacroTests[i].line);
    buf_seek(line, 0);
    rc = parse_unbind(&UnMacro, line, err);
    TEST_CHECK_NUM_EQ(rc, UnMacroTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

void test_parse_unbind(void)
{
  test_parse_unbind2();
  test_parse_unmacro();
}
