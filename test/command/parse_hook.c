/**
 * @file
 * Test code for parse_hook()
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
static const struct Command AccountHook  = { "account-hook",  NULL, MUTT_ACCOUNT_HOOK };
static const struct Command AppendHook   = { "append-hook",   NULL, MUTT_APPEND_HOOK };
static const struct Command CloseHook    = { "close-hook",    NULL, MUTT_CLOSE_HOOK };
static const struct Command CryptHook    = { "crypt-hook",    NULL, MUTT_CRYPT_HOOK };
static const struct Command FccHook      = { "fcc-hook",      NULL, MUTT_FCC_HOOK };
static const struct Command FccSaveHook  = { "fcc-save-hook", NULL, MUTT_FCC_HOOK | MUTT_SAVE_HOOK };
static const struct Command FolderHook   = { "folder-hook",   NULL, MUTT_FOLDER_HOOK };
static const struct Command MboxHook     = { "mbox-hook",     NULL, MUTT_MBOX_HOOK };
static const struct Command MessageHook  = { "message-hook",  NULL, MUTT_MESSAGE_HOOK };
static const struct Command OpenHook     = { "open-hook",     NULL, MUTT_OPEN_HOOK };
static const struct Command ReplyHook    = { "reply-hook",    NULL, MUTT_REPLY_HOOK };
static const struct Command SaveHook     = { "save-hook",     NULL, MUTT_SAVE_HOOK };
static const struct Command Send2Hook    = { "send2-hook",    NULL, MUTT_SEND2_HOOK };
static const struct Command SendHook     = { "send-hook",     NULL, MUTT_SEND_HOOK };
static const struct Command ShutdownHook = { "shutdown-hook", NULL, MUTT_SHUTDOWN_HOOK | MUTT_GLOBAL_HOOK };
static const struct Command StartupHook  = { "startup-hook",  NULL, MUTT_STARTUP_HOOK | MUTT_GLOBAL_HOOK };
static const struct Command TimeoutHook  = { "timeout-hook",  NULL, MUTT_TIMEOUT_HOOK | MUTT_GLOBAL_HOOK };
// clang-format on

// clang-format off
static const struct CommandTest AccountTests[] = {
  // account-hook <regex> <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, ". 'unset imap_user; unset imap_pass; unset tunnel'" },
  { MUTT_CMD_SUCCESS, "imap://host1/ 'set imap_user=me1 imap_pass=foo'" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest AppendTests[] = {
  // append-hook <regex> "<shell-command>"
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'\\.gz$' \"gzip --stdout              '%t' >> '%f'\"" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest CloseTests[] = {
  // close-hook <regex> "<shell-command>"
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'\\.gz$' \"gzip --stdout              '%t' >  '%f'\"" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest CryptTests[] = {
  // crypt-hook <regex> <keyid>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'.'              0x1111111111222222222233333333334444444444" },
  { MUTT_CMD_SUCCESS, "'.*@example.com' 0xAAAAAAAAAABBBBBBBBBBCCCCCCCCCCDDDDDDDDDD" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest FccTests[] = {
  // fcc-hook <pattern> <mailbox>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "[@.]aol\\.com$ +spammers" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest FccSaveTests[] = {
  // fcc-save-hook <pattern> <mailbox>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'~t neomutt-users*' +Lists/neomutt-users" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest FolderTests[] = {
  // folder-hook [ -noregex ] <regex> <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, ".             'set sort=date-sent'" },
  { MUTT_CMD_SUCCESS, "-noregex work 'set sort=threads'" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest MboxTests[] = {
  // mbox-hook [ -noregex ] <regex> <mailbox>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'.*example\\.com' '+work'" },
  { MUTT_CMD_SUCCESS, "-noregex 'example\\.com' '+other'" },
  { MUTT_CMD_ERROR,   NULL },
};
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
// clang-format on

// clang-format off
static const struct CommandTest OpenTests[] = {
  // open-hook <regex> "<shell-command>"
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'\\.gz$' \"gzip --stdout --decompress '%f' >  '%t'\"" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest ReplyTests[] = {
  // reply-hook <pattern> <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, ". 'set from=\"Dave Jones <dave@jones.com>\"'" },
  { MUTT_CMD_SUCCESS, "'~s neomutt' 'set signature=\"~/.sig\"'" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest SaveTests[] = {
  // save-hook <pattern> <mailbox>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'~f root@localhost' =Temp/rootmail" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest SendTests[] = {
  // send-hook <pattern> <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "~A 'set signature=\"~/.sig\"'" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest Send2Tests[] = {
  // send2-hook <pattern> <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'~s neomutt' 'my_hdr X-Custom: hello world'" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest ShutdownTests[] = {
  // shutdown-hook <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'<shell-escape>touch ~/test<enter>'" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest StartupTests[] = {
  // startup-hook <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'exec sync-mailbox'" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest TimeoutTests[] = {
  // timeout-hook <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'exec sync-mailbox'" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

static void test_parse_account_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; AccountTests[i].line; i++)
  {
    TEST_CASE(AccountTests[i].line);
    buf_reset(err);
    buf_strcpy(line, AccountTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_regex(&AccountHook, line, err);
    TEST_CHECK_NUM_EQ(rc, AccountTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_append_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; AppendTests[i].line; i++)
  {
    TEST_CASE(AppendTests[i].line);
    buf_reset(err);
    buf_strcpy(line, AppendTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_compress(&AppendHook, line, err);
    TEST_CHECK_NUM_EQ(rc, AppendTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_close_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; CloseTests[i].line; i++)
  {
    TEST_CASE(CloseTests[i].line);
    buf_reset(err);
    buf_strcpy(line, CloseTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_compress(&CloseHook, line, err);
    TEST_CHECK_NUM_EQ(rc, CloseTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_crypt_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; CryptTests[i].line; i++)
  {
    TEST_CASE(CryptTests[i].line);
    buf_reset(err);
    buf_strcpy(line, CryptTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_crypt(&CryptHook, line, err);
    TEST_CHECK_NUM_EQ(rc, CryptTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_fcc_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; FccTests[i].line; i++)
  {
    TEST_CASE(FccTests[i].line);
    buf_reset(err);
    buf_strcpy(line, FccTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_mailbox(&FccHook, line, err);
    TEST_CHECK_NUM_EQ(rc, FccTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_fcc_save_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; FccSaveTests[i].line; i++)
  {
    TEST_CASE(FccSaveTests[i].line);
    buf_reset(err);
    buf_strcpy(line, FccSaveTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_mailbox(&FccSaveHook, line, err);
    TEST_CHECK_NUM_EQ(rc, FccSaveTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_folder_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; FolderTests[i].line; i++)
  {
    TEST_CASE(FolderTests[i].line);
    buf_reset(err);
    buf_strcpy(line, FolderTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_folder(&FolderHook, line, err);
    TEST_CHECK_NUM_EQ(rc, FolderTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_mbox_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; MboxTests[i].line; i++)
  {
    TEST_CASE(MboxTests[i].line);
    buf_reset(err);
    buf_strcpy(line, MboxTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_mbox(&MboxHook, line, err);
    TEST_CHECK_NUM_EQ(rc, MboxTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_message_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

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

static void test_parse_open_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; OpenTests[i].line; i++)
  {
    TEST_CASE(OpenTests[i].line);
    buf_reset(err);
    buf_strcpy(line, OpenTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_compress(&OpenHook, line, err);
    TEST_CHECK_NUM_EQ(rc, OpenTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_reply_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

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

static void test_parse_save_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; SaveTests[i].line; i++)
  {
    TEST_CASE(SaveTests[i].line);
    buf_reset(err);
    buf_strcpy(line, SaveTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_mailbox(&SaveHook, line, err);
    TEST_CHECK_NUM_EQ(rc, SaveTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_send_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; Send2Tests[i].line; i++)
  {
    TEST_CASE(Send2Tests[i].line);
    buf_reset(err);
    buf_strcpy(line, Send2Tests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_pattern(&SendHook, line, err);
    TEST_CHECK_NUM_EQ(rc, Send2Tests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_send2_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; SendTests[i].line; i++)
  {
    TEST_CASE(SendTests[i].line);
    buf_reset(err);
    buf_strcpy(line, SendTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_pattern(&Send2Hook, line, err);
    TEST_CHECK_NUM_EQ(rc, SendTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_shutdown_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; ShutdownTests[i].line; i++)
  {
    TEST_CASE(ShutdownTests[i].line);
    buf_reset(err);
    buf_strcpy(line, ShutdownTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_global(&ShutdownHook, line, err);
    TEST_CHECK_NUM_EQ(rc, ShutdownTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_startup_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; StartupTests[i].line; i++)
  {
    TEST_CASE(StartupTests[i].line);
    buf_reset(err);
    buf_strcpy(line, StartupTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_global(&StartupHook, line, err);
    TEST_CHECK_NUM_EQ(rc, StartupTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_timeout_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; TimeoutTests[i].line; i++)
  {
    TEST_CASE(TimeoutTests[i].line);
    buf_reset(err);
    buf_strcpy(line, TimeoutTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_global(&TimeoutHook, line, err);
    TEST_CHECK_NUM_EQ(rc, TimeoutTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

void test_parse_hook(void)
{
  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars));

  test_parse_account_hook();
  test_parse_append_hook();
  test_parse_close_hook();
  test_parse_crypt_hook();
  test_parse_fcc_hook();
  test_parse_fcc_save_hook();
  test_parse_folder_hook();
  test_parse_mbox_hook();
  test_parse_message_hook();
  test_parse_open_hook();
  test_parse_reply_hook();
  test_parse_save_hook();
  test_parse_send2_hook();
  test_parse_send_hook();
  test_parse_shutdown_hook();
  test_parse_startup_hook();
  test_parse_timeout_hook();
}
