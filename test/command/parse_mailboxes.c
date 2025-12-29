/**
 * @file
 * Test code for parse_mailboxes()
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
#include "commands.h"
#include "common.h"
#include "test_common.h"

// clang-format off
static const struct Command Mailboxes      = { "mailboxes",       CMD_MAILBOXES,       NULL, CMD_NO_DATA };
static const struct Command NamedMailboxes = { "named-mailboxes", CMD_NAMED_MAILBOXES, NULL, CMD_NO_DATA };
// clang-format on

// clang-format off
static const struct CommandTest MailboxesTests[] = {
  // mailboxes [[ -label <label> ] | -nolabel ] [[ -notify | -nonotify ] [ -poll | -nopoll ] <mailbox> ] [ ... ]
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "+" },
  { MUTT_CMD_SUCCESS, "+neo" },
  { MUTT_CMD_SUCCESS, "+neo/devel" },
  { MUTT_CMD_SUCCESS, "+neo/github" },
  { MUTT_CMD_SUCCESS, "-label apple +home/apple" },
  { MUTT_CMD_SUCCESS, "-nolabel     +home/apple" },
  { MUTT_CMD_SUCCESS, "-notify   +home/banana" },
  { MUTT_CMD_SUCCESS, "-nonotify +home/banana" },
  { MUTT_CMD_SUCCESS, "-poll   +home/cherry" },
  { MUTT_CMD_SUCCESS, "-nopoll +home/cherry" },
  { MUTT_CMD_SUCCESS, "+home/damson +home/endive -label f +home/fig" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest NamedMailboxesTests[] = {
  // named-mailboxes <description> <mailbox> [ <description> <mailbox> ... ]
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "apple +home/apple" },
  { MUTT_CMD_SUCCESS, "banana +home/banana cherry +home/cherry" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

static void test_parse_mailboxes2(void)
{
  // enum CommandResult parse_mailboxes(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; MailboxesTests[i].line; i++)
  {
    TEST_CASE(MailboxesTests[i].line);
    buf_reset(err);
    buf_strcpy(line, MailboxesTests[i].line);
    buf_seek(line, 0);
    rc = parse_mailboxes(&Mailboxes, line, err);
    TEST_CHECK_NUM_EQ(rc, MailboxesTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_named_mailboxes(void)
{
  // enum CommandResult parse_mailboxes(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; NamedMailboxesTests[i].line; i++)
  {
    TEST_CASE(NamedMailboxesTests[i].line);
    buf_reset(err);
    buf_strcpy(line, NamedMailboxesTests[i].line);
    buf_seek(line, 0);
    rc = parse_mailboxes(&NamedMailboxes, line, err);
    TEST_CHECK_NUM_EQ(rc, NamedMailboxesTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

void test_parse_mailboxes(void)
{
  test_parse_mailboxes2();
  test_parse_named_mailboxes();
}
