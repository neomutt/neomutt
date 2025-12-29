/**
 * @file
 * Test code for parse_hook_regex()
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
#include "hooks/lib.h"
#include "common.h"
#include "test_common.h"

// clang-format off
static const struct Command AccountHook = { "account-hook", CMD_ACCOUNT_HOOK, NULL };
// clang-format on

// clang-format off
static const struct CommandTest Tests[] = {
  // account-hook <regex> <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, ". 'unset imap_user; unset imap_pass; unset tunnel'" },
  { MUTT_CMD_SUCCESS, "imap://host1/ 'set imap_user=me1 imap_pass=foo'" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

void test_parse_hook_regex(void)
{
  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; Tests[i].line; i++)
  {
    TEST_CASE(Tests[i].line);
    buf_reset(err);
    buf_strcpy(line, Tests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_regex(&AccountHook, line, err);
    TEST_CHECK_NUM_EQ(rc, Tests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}
