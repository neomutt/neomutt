/**
 * @file
 * Test code for parse_stailq()
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
#include "commands/lib.h"
#include "common.h"
#include "globals.h"
#include "test_common.h"

// clang-format off
static const struct Command AlternativeOrder = { "alternative-order", CMD_ALTERNATIVE_ORDER, NULL, IP &AlternativeOrderList };
static const struct Command AutoView         = { "auto-view",         CMD_AUTO_VIEW,         NULL, IP &AutoViewList         };
static const struct Command HdrOrder         = { "hdr-order",         CMD_HDR_ORDER,         NULL, IP &HeaderOrderList      };
static const struct Command MailtoAllow      = { "mailto-allow",      CMD_MAILTO_ALLOW,      NULL, IP &MailToAllow          };
static const struct Command MimeLookup       = { "mime-lookup",       CMD_MIME_LOOKUP,       NULL, IP &MimeLookupList       };
// clang-format off

// clang-format off
static const struct CommandTest AlternativeOrderTests[] = {
  // alternative-order <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "text/enriched text/plain text application/postscript image/*" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest AutoViewTests[] = {
  // auto-view <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "text/html application/x-gunzip image/gif application/x-tar-gz" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest HdrOrderTests[] = {
  // hdr_order <header> [ <header> ... ]
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "From Date: From: To: Cc: Subject:" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest MailtoAllowTests[] = {
  // mailto-allow { * | <header-field> ... }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "bcc" },
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest MimeLookupTests[] = {
  // mime-lookup <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "application/octet-stream application/X-Lotus-Manuscript" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

static void alternative_order(void)
{
  // enum CommandResult parse_stailq(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; AlternativeOrderTests[i].line; i++)
  {
    TEST_CASE(AlternativeOrderTests[i].line);
    buf_reset(err);
    buf_strcpy(line, AlternativeOrderTests[i].line);
    buf_seek(line, 0);
    rc = parse_stailq(&AlternativeOrder, line, err);
    TEST_CHECK_NUM_EQ(rc, AlternativeOrderTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void auto_view(void)
{
  // enum CommandResult parse_stailq(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; AutoViewTests[i].line; i++)
  {
    TEST_CASE(AutoViewTests[i].line);
    buf_reset(err);
    buf_strcpy(line, AutoViewTests[i].line);
    buf_seek(line, 0);
    rc = parse_stailq(&AutoView, line, err);
    TEST_CHECK_NUM_EQ(rc, AutoViewTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void hdr_order(void)
{
  // enum CommandResult parse_stailq(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; HdrOrderTests[i].line; i++)
  {
    TEST_CASE(HdrOrderTests[i].line);
    buf_reset(err);
    buf_strcpy(line, HdrOrderTests[i].line);
    buf_seek(line, 0);
    rc = parse_stailq(&HdrOrder, line, err);
    TEST_CHECK_NUM_EQ(rc, HdrOrderTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void mailto_allow(void)
{
  // enum CommandResult parse_stailq(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; MailtoAllowTests[i].line; i++)
  {
    TEST_CASE(MailtoAllowTests[i].line);
    buf_reset(err);
    buf_strcpy(line, MailtoAllowTests[i].line);
    buf_seek(line, 0);
    rc = parse_stailq(&MailtoAllow, line, err);
    TEST_CHECK_NUM_EQ(rc, MailtoAllowTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void mime_lookup(void)
{
  // enum CommandResult parse_stailq(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; MimeLookupTests[i].line; i++)
  {
    TEST_CASE(MimeLookupTests[i].line);
    buf_reset(err);
    buf_strcpy(line, MimeLookupTests[i].line);
    buf_seek(line, 0);
    rc = parse_stailq(&MimeLookup, line, err);
    TEST_CHECK_NUM_EQ(rc, MimeLookupTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

void test_parse_stailq(void)
{
  alternative_order();
  auto_view();
  hdr_order();
  mailto_allow();
  mime_lookup();
}
