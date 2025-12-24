/**
 * @file
 * Test code for parse_hook_compress()
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
static const struct Command AppendHook = { "append-hook", NULL, MUTT_APPEND_HOOK };
static const struct Command CloseHook  = { "close-hook",  NULL, MUTT_CLOSE_HOOK };
static const struct Command OpenHook   = { "open-hook",   NULL, MUTT_OPEN_HOOK };
// clang-format on

// clang-format off
static const struct CommandTest AppendTests[] = {
  // append-hook <regex> "<shell-command>"
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'\\.gz$' \"gzip --stdout              '%t' >> '%f'\"" },
  { MUTT_CMD_ERROR,   NULL },
};

static const struct CommandTest CloseTests[] = {
  // close-hook <regex> "<shell-command>"
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'\\.gz$' \"gzip --stdout              '%t' >  '%f'\"" },
  { MUTT_CMD_ERROR,   NULL },
};

static const struct CommandTest OpenTests[] = {
  // open-hook <regex> "<shell-command>"
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'\\.gz$' \"gzip --stdout --decompress '%f' >  '%t'\"" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

static void test_parse_append_hook(void)
{
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

static void test_parse_open_hook(void)
{
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

void test_parse_hook_compress(void)
{
  test_parse_append_hook();
  test_parse_close_hook();
  test_parse_open_hook();
}
