/**
 * @file
 * Test code for parse_unstailq()
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
#include "email/lib.h"
#include "core/lib.h"
#include "commands/lib.h"
#include "parse/lib.h"
#include "common.h"
#include "globals.h"
#include "test_common.h"

// clang-format off
static const struct Command UnMimeLookup       = { "unmime-lookup",       CMD_UNMIME_LOOKUP,       NULL, IP &MimeLookupList       };
// clang-format on

static const struct CommandTest UnAlternativeOrderTests[] = {
  // clang-format off
  // unalternative-order { * | [ <mime-type>[/<mime-subtype> ] ... ] }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "text/enriched text/plain text application/postscript image/*" },
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static const struct CommandTest UnAutoViewTests[] = {
  // clang-format off
  // unauto-view { * | [ <mime-type>[/<mime-subtype> ] ... ] }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "text/html application/x-gunzip image/gif application/x-tar-gz" },
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static const struct CommandTest UnHdrOrderTests[] = {
  // clang-format off
  // unheader-order { * | <header> ... }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "From Date: From: To: Cc: Subject:" },
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static const struct CommandTest UnMailtoAllowTests[] = {
  // clang-format off
  // unmailto-allow { * | <header-field> ... }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "bcc" },
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static const struct CommandTest UnMimeLookupTests[] = {
  // clang-format off
  // unmime-lookup { * | [ <mime-type>[/<mime-subtype> ] ... ] }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "application/octet-stream application/X-Lotus-Manuscript" },
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static void test_parse_unalternative_order(void)
{
  // enum CommandResult parse_unstailq(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  const struct Command *cmd = command_find_by_id(&NeoMutt->commands, CMD_UNALTERNATIVE_ORDER);
  TEST_CHECK(cmd != NULL);

  for (int i = 0; UnAlternativeOrderTests[i].line; i++)
  {
    TEST_CASE(UnAlternativeOrderTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, UnAlternativeOrderTests[i].line);
    buf_seek(line, 0);
    rc = parse_unlist(cmd, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, UnAlternativeOrderTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

static void test_parse_unauto_view(void)
{
  // enum CommandResult parse_unstailq(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  const struct Command *cmd = command_find_by_id(&NeoMutt->commands, CMD_UNAUTO_VIEW);
  TEST_CHECK(cmd != NULL);

  for (int i = 0; UnAutoViewTests[i].line; i++)
  {
    TEST_CASE(UnAutoViewTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, UnAutoViewTests[i].line);
    buf_seek(line, 0);
    rc = parse_unlist(cmd, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, UnAutoViewTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

static void test_parse_unhdr_order(void)
{
  // enum CommandResult parse_unstailq(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  const struct Command *cmd = command_find_by_id(&NeoMutt->commands, CMD_UNHEADER_ORDER);
  TEST_CHECK(cmd != NULL);

  for (int i = 0; UnHdrOrderTests[i].line; i++)
  {
    TEST_CASE(UnHdrOrderTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, UnHdrOrderTests[i].line);
    buf_seek(line, 0);
    rc = parse_unlist(cmd, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, UnHdrOrderTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

static void test_parse_unmailto_allow(void)
{
  // enum CommandResult parse_unstailq(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  const struct Command *cmd = command_find_by_id(&NeoMutt->commands, CMD_UNMAILTO_ALLOW);
  TEST_CHECK(cmd != NULL);

  for (int i = 0; UnMailtoAllowTests[i].line; i++)
  {
    TEST_CASE(UnMailtoAllowTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, UnMailtoAllowTests[i].line);
    buf_seek(line, 0);
    rc = parse_unlist(cmd, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, UnMailtoAllowTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

static void test_parse_unmime_lookup(void)
{
  // enum CommandResult parse_unstailq(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; UnMimeLookupTests[i].line; i++)
  {
    TEST_CASE(UnMimeLookupTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, UnMimeLookupTests[i].line);
    buf_seek(line, 0);
    rc = parse_unstailq(&UnMimeLookup, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, UnMimeLookupTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

void test_parse_unstailq(void)
{
  test_parse_unalternative_order();
  test_parse_unauto_view();
  test_parse_unhdr_order();
  test_parse_unmailto_allow();
  test_parse_unmime_lookup();
}
