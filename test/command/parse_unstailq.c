/**
 * @file
 * Test code for parse_unstailq()
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
#include "email/lib.h"
#include "core/lib.h"
#include "commands.h"
#include "common.h"
#include "globals.h"
#include "test_common.h"

// clang-format off
static const struct Command UnAlternativeOrder = { "unalternative_order", NULL, IP &AlternativeOrderList };
static const struct Command UnAutoView         = { "unauto_view",         NULL, IP &AutoViewList };
static const struct Command UnHdrOrder         = { "unhdr_order",         NULL, IP &HeaderOrderList };
static const struct Command UnMailtoAllow      = { "unmailto_allow",      NULL, IP &MailToAllow };
static const struct Command UnMimeLookup       = { "unmime_lookup",       NULL, IP &MimeLookupList };
// clang-format on

// clang-format off
static const struct CommandTest UnAlternativeOrderTests[] = {
  // unalternative_order { * | [ <mime-type>[/<mime-subtype> ] ... ] }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "text/enriched text/plain text application/postscript image/*" },
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest UnAutoViewTests[] = {
  // unauto_view { * | [ <mime-type>[/<mime-subtype> ] ... ] }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "text/html application/x-gunzip image/gif application/x-tar-gz" },
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest UnHdrOrderTests[] = {
  // unhdr_order { * | <header> ... }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "From Date: From: To: Cc: Subject:" },
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest UnMailtoAllowTests[] = {
  // unmailto_allow { * | <header-field> ... }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "bcc" },
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest UnMimeLookupTests[] = {
  // unmime_lookup { * | [ <mime-type>[/<mime-subtype> ] ... ] }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "application/octet-stream application/X-Lotus-Manuscript" },
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

static void test_parse_unalternative_order(void)
{
  // enum CommandResult parse_unstailq(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; UnAlternativeOrderTests[i].line; i++)
  {
    TEST_CASE(UnAlternativeOrderTests[i].line);
    buf_reset(err);
    buf_strcpy(line, UnAlternativeOrderTests[i].line);
    buf_seek(line, 0);
    rc = parse_unstailq(&UnAlternativeOrder, line, err);
    TEST_CHECK_NUM_EQ(rc, UnAlternativeOrderTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_unauto_view(void)
{
  // enum CommandResult parse_unstailq(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; UnAutoViewTests[i].line; i++)
  {
    TEST_CASE(UnAutoViewTests[i].line);
    buf_reset(err);
    buf_strcpy(line, UnAutoViewTests[i].line);
    buf_seek(line, 0);
    rc = parse_unstailq(&UnAutoView, line, err);
    TEST_CHECK_NUM_EQ(rc, UnAutoViewTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_unhdr_order(void)
{
  // enum CommandResult parse_unstailq(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; UnHdrOrderTests[i].line; i++)
  {
    TEST_CASE(UnHdrOrderTests[i].line);
    buf_reset(err);
    buf_strcpy(line, UnHdrOrderTests[i].line);
    buf_seek(line, 0);
    rc = parse_unstailq(&UnHdrOrder, line, err);
    TEST_CHECK_NUM_EQ(rc, UnHdrOrderTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_unmailto_allow(void)
{
  // enum CommandResult parse_unstailq(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; UnMailtoAllowTests[i].line; i++)
  {
    TEST_CASE(UnMailtoAllowTests[i].line);
    buf_reset(err);
    buf_strcpy(line, UnMailtoAllowTests[i].line);
    buf_seek(line, 0);
    rc = parse_unstailq(&UnMailtoAllow, line, err);
    TEST_CHECK_NUM_EQ(rc, UnMailtoAllowTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_unmime_lookup(void)
{
  // enum CommandResult parse_unstailq(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; UnMimeLookupTests[i].line; i++)
  {
    TEST_CASE(UnMimeLookupTests[i].line);
    buf_reset(err);
    buf_strcpy(line, UnMimeLookupTests[i].line);
    buf_seek(line, 0);
    rc = parse_unstailq(&UnMimeLookup, line, err);
    TEST_CHECK_NUM_EQ(rc, UnMimeLookupTests[i].rc);
  }

  buf_pool_release(&err);
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
