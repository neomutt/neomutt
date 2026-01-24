/**
 * @file
 * Test code for parse_mailbox_hook()
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
#include "hooks/lib.h"
#include "parse/lib.h"
#include "common.h"
#include "test_common.h"

static struct ConfigDef Vars[] = {
  // clang-format off
  { "default_hook", DT_STRING, IP "~f %s !~P | (~P ~C %s)", 0, NULL, },
  { NULL },
  // clang-format on
};

// clang-format off
static const struct Command FccHook     = { "fcc-hook",      CMD_FCC_HOOK,      NULL, CMD_NO_DATA };
static const struct Command FccSaveHook = { "fcc-save-hook", CMD_FCC_SAVE_HOOK, NULL, CMD_NO_DATA };
static const struct Command SaveHook    = { "save-hook",     CMD_SAVE_HOOK,     NULL, CMD_NO_DATA };
// clang-format on

static const struct CommandTest FccTests[] = {
  // clang-format off
  // fcc-hook      <pattern> <mailbox>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "[@.]aol\\.com$ +spammers" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static const struct CommandTest FccSaveTests[] = {
  // clang-format off
  // fcc-save-hook <pattern> <mailbox>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'~t neomutt-users*' +Lists/neomutt-users" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static const struct CommandTest SaveTests[] = {
  // clang-format off
  // save-hook     <pattern> <mailbox>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'~f root@localhost' =Temp/rootmail" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static void test_parse_fcc_hook(void)
{
  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; FccTests[i].line; i++)
  {
    TEST_CASE(FccTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, FccTests[i].line);
    buf_seek(line, 0);
    rc = parse_mailbox_hook(&FccHook, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, FccTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

static void test_parse_fcc_save_hook(void)
{
  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; FccSaveTests[i].line; i++)
  {
    TEST_CASE(FccSaveTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, FccSaveTests[i].line);
    buf_seek(line, 0);
    rc = parse_mailbox_hook(&FccSaveHook, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, FccSaveTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

static void test_parse_save_hook(void)
{
  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; SaveTests[i].line; i++)
  {
    TEST_CASE(SaveTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, SaveTests[i].line);
    buf_seek(line, 0);
    rc = parse_mailbox_hook(&SaveHook, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, SaveTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

void test_parse_hook_mailbox(void)
{
  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars));

  test_parse_fcc_hook();
  test_parse_fcc_save_hook();
  test_parse_save_hook();
}
