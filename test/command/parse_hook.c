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
#include "core/lib.h"
#include "common.h"
#include "hook.h"
#include "test_common.h"

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
static const struct CommandTest Tests[] = {
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

void test_parse_hook(void)
{
  // enum CommandResult parse_hook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; Tests[i].line; i++)
  {
    TEST_CASE(Tests[i].line);
    buf_reset(err);
    buf_strcpy(line, Tests[i].line);
    buf_seek(line, 0);
    rc = parse_hook(&TimeoutHook, line, err);
    TEST_CHECK_NUM_EQ(rc, Tests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}
