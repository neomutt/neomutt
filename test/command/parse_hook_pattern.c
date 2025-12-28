/**
 * @file
 * Test code for parse_hook_pattern()
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

static struct ConfigDef Vars[] = {
  // clang-format off
  { "default_hook", DT_STRING, IP "~f %s !~P | (~P ~C %s)", 0, NULL, },
  { NULL },
  // clang-format on
};

// clang-format off
static const struct Command MessageHook = { "message-hook", CMD_MESSAGE_HOOK, NULL };
static const struct Command ReplyHook   = { "reply-hook",   CMD_REPLY_HOOK,   NULL };
static const struct Command SendHook    = { "send-hook",    CMD_SEND_HOOK,    NULL };
static const struct Command Send2Hook   = { "send2-hook",   CMD_SEND2_HOOK,   NULL };
// clang-format on

// clang-format off
static const struct CommandTest MessageTests[] = {
  // message-hook <pattern> <command>
  { MUTT_CMD_SUCCESS, "~g 'set my_var=42'" },
  { MUTT_CMD_SUCCESS, ". 'color header default default (Date|From|To)'" },
  { MUTT_CMD_SUCCESS, "'~h bob' 'set signature=\"~/.sig\"'" },
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_ERROR,   NULL },
};

static const struct CommandTest ReplyTests[] = {
  // reply-hook <pattern> <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, ". 'set from=\"Dave Jones <dave@jones.com>\"'" },
  { MUTT_CMD_SUCCESS, "'~s neomutt' 'set signature=\"~/.sig\"'" },
  { MUTT_CMD_ERROR,   NULL },
};

static const struct CommandTest SendTests[] = {
  // send-hook <pattern> <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "~A 'set signature=\"~/.sig\"'" },
  { MUTT_CMD_ERROR,   NULL },
};

static const struct CommandTest Send2Tests[] = {
  // send2-hook <pattern> <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'~s neomutt' 'my_hdr X-Custom: hello world'" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

static void test_parse_message_hook(void)
{
  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; MessageTests[i].line; i++)
  {
    TEST_CASE(MessageTests[i].line);
    buf_reset(err);
    buf_strcpy(line, MessageTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_pattern(&MessageHook, line, err);
    TEST_CHECK_NUM_EQ(rc, MessageTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_reply_hook(void)
{
  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; ReplyTests[i].line; i++)
  {
    TEST_CASE(ReplyTests[i].line);
    buf_reset(err);
    buf_strcpy(line, ReplyTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_pattern(&ReplyHook, line, err);
    TEST_CHECK_NUM_EQ(rc, ReplyTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_send_hook(void)
{
  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; SendTests[i].line; i++)
  {
    TEST_CASE(SendTests[i].line);
    buf_reset(err);
    buf_strcpy(line, SendTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_pattern(&SendHook, line, err);
    TEST_CHECK_NUM_EQ(rc, SendTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_send2_hook(void)
{
  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; Send2Tests[i].line; i++)
  {
    TEST_CASE(Send2Tests[i].line);
    buf_reset(err);
    buf_strcpy(line, Send2Tests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_pattern(&Send2Hook, line, err);
    TEST_CHECK_NUM_EQ(rc, Send2Tests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

void test_parse_hook_pattern(void)
{
  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars));

  test_parse_message_hook();
  test_parse_reply_hook();
  test_parse_send_hook();
  test_parse_send2_hook();
}
