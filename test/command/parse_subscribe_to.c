/**
 * @file
 * Test code for parse_subscribe_to()
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
#include "config/lib.h"
#include "core/lib.h"
#include "imap/lib.h"
#include "parse/lib.h"
#include "common.h"
#include "test_common.h"

static const struct Command SubscribeTo = { "subscribe-to", CMD_SUBSCRIBE_TO, NULL };

static const struct CommandTest Tests[] = {
  // clang-format off
  // subscribe-to <imap-folder-uri>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_ERROR,   "imaps://mail.example.org/inbox" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static struct ConfigDef Vars[] = {
  // clang-format off
  { "imap_delim_chars", DT_STRING, IP "/.", 0, NULL, },
  { NULL },
  // clang-format on
};

void test_parse_subscribe_to(void)
{
  // enum CommandResult parse_subscribe_to(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars));

  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; Tests[i].line; i++)
  {
    TEST_CASE(Tests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, Tests[i].line);
    buf_seek(line, 0);
    rc = parse_subscribe_to(&SubscribeTo, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, Tests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}
