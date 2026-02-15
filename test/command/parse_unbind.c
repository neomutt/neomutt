/**
 * @file
 * Test code for parse_unbind()
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
#include "index/lib.h"
#include "key/lib.h"
#include "pager/lib.h"
#include "parse/lib.h"
#include "sidebar/lib.h"
#include "common.h"
#include "test_common.h"

// clang-format off
static const struct Command UnBind  = { "unbind",  CMD_UNBIND,  NULL};
static const struct Command UnMacro = { "unmacro", CMD_UNMACRO, NULL};
// clang-format on

static const struct CommandTest UnBindTests[] = {
  // clang-format off
  // unbind { * | <map>[,<map> ... ] } [ <key> ]
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_SUCCESS, "* d" },
  { MUTT_CMD_SUCCESS, "* missing" },
  { MUTT_CMD_SUCCESS, "index" },
  { MUTT_CMD_SUCCESS, "index,pager" },
  { MUTT_CMD_SUCCESS, "index *" },
  { MUTT_CMD_SUCCESS, "index,pager *" },
  { MUTT_CMD_SUCCESS, "index d" },
  { MUTT_CMD_SUCCESS, "index missing" },
  { MUTT_CMD_SUCCESS, "index,pager d" },
  { MUTT_CMD_SUCCESS, "index,pager missing" },
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_WARNING, "bad" },
  { MUTT_CMD_WARNING, "bad,index" },
  { MUTT_CMD_WARNING, "index,bad" },
  { MUTT_CMD_WARNING, "index d extra" },
  { MUTT_CMD_WARNING, "index,pager d extra" },
  { MUTT_CMD_WARNING, "* d extra" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static const struct CommandTest UnMacroTests[] = {
  // clang-format off
  // unmacro { * | <map>[,<map> ... ] } [ <key> ]
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "index eee" },
  { MUTT_CMD_SUCCESS, "index nn" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static void init_menus(void)
{
  struct SubMenu *sm_generic = generic_init_keys();

  sidebar_init_keys(sm_generic);
  index_init_keys(sm_generic);
  pager_init_keys(sm_generic);
}

static void test_parse_unbind2(void)
{
  // enum CommandResult parse_unbind(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; UnBindTests[i].line; i++)
  {
    init_menus();

    TEST_CASE(UnBindTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, UnBindTests[i].line);
    buf_seek(line, 0);
    rc = parse_unbind(&UnBind, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, UnBindTests[i].rc);

    km_cleanup();
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

static void test_parse_unmacro(void)
{
  // enum CommandResult parse_unbind(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  init_menus();
  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; UnMacroTests[i].line; i++)
  {
    TEST_CASE(UnMacroTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, UnMacroTests[i].line);
    buf_seek(line, 0);
    rc = parse_unbind(&UnMacro, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, UnMacroTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
  km_cleanup();
}

void test_parse_unbind(void)
{
  test_parse_unbind2();
  test_parse_unmacro();
}
