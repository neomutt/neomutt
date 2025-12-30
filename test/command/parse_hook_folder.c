/**
 * @file
 * Test code for parse_hook_folder()
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
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "hooks/lib.h"
#include "common.h"
#include "test_common.h"

// clang-format off
static const struct Command FolderHook = { "folder-hook", CMD_FOLDER_HOOK, NULL };
// clang-format on

// clang-format off
static const struct CommandTest Tests[] = {
  // folder-hook [ -noregex ] <regex> <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, ".             'set sort=date-sent'" },
  { MUTT_CMD_SUCCESS, "-noregex work 'set sort=threads'" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

void test_parse_hook_folder(void)
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
    rc = parse_hook_folder(&FolderHook, line, err);
    TEST_CHECK_NUM_EQ(rc, Tests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);

  // Tests for parse_folder_hook_line function
  struct HookParseError error = { 0 };
  struct FolderHookData data = { 0 };
  bool result;

  // Test: NULL line
  {
    TEST_CASE("NULL line");
    memset(&data, 0, sizeof(data));
    memset(&error, 0, sizeof(error));
    result = parse_folder_hook_line(NULL, &data, &error);
    TEST_CHECK(result == false);
    TEST_CHECK(error.message != NULL);
    TEST_CHECK(error.position == 0);
    folder_hook_data_free(&data);
  }

  // Test: NULL data
  {
    TEST_CASE("NULL data");
    memset(&error, 0, sizeof(error));
    result = parse_folder_hook_line(".", NULL, &error);
    TEST_CHECK(result == false);
    TEST_CHECK(error.message != NULL);
  }

  // Test: Empty line
  {
    TEST_CASE("Empty line");
    memset(&data, 0, sizeof(data));
    memset(&error, 0, sizeof(error));
    result = parse_folder_hook_line("", &data, &error);
    TEST_CHECK(result == false);
    TEST_CHECK(error.message != NULL);
    folder_hook_data_free(&data);
  }

  // Test: Only whitespace
  {
    TEST_CASE("Only whitespace");
    memset(&data, 0, sizeof(data));
    memset(&error, 0, sizeof(error));
    result = parse_folder_hook_line("   ", &data, &error);
    TEST_CHECK(result == false);
    TEST_CHECK(error.message != NULL);
    folder_hook_data_free(&data);
  }

  // Test: Only regex, no command
  {
    TEST_CASE("Only regex, no command");
    memset(&data, 0, sizeof(data));
    memset(&error, 0, sizeof(error));
    result = parse_folder_hook_line(".", &data, &error);
    TEST_CHECK(result == false);
    TEST_CHECK(error.message != NULL);
    folder_hook_data_free(&data);
  }

  // Test: Valid simple hook
  {
    TEST_CASE("Valid simple hook");
    memset(&data, 0, sizeof(data));
    memset(&error, 0, sizeof(error));
    result = parse_folder_hook_line(". 'set sort=date-sent'", &data, &error);
    TEST_CHECK(result == true);
    if (result)
    {
      TEST_CHECK_STR_EQ(data.regex, ".");
      TEST_CHECK_STR_EQ(data.command, "set sort=date-sent");
      TEST_CHECK(data.pat_not == false);
      TEST_CHECK(data.use_regex == true);
    }
    folder_hook_data_free(&data);
  }

  // Test: Valid hook with negation
  {
    TEST_CASE("Valid hook with negation");
    memset(&data, 0, sizeof(data));
    memset(&error, 0, sizeof(error));
    result = parse_folder_hook_line("! . 'set sort=threads'", &data, &error);
    TEST_CHECK(result == true);
    if (result)
    {
      TEST_CHECK_STR_EQ(data.regex, ".");
      TEST_CHECK_STR_EQ(data.command, "set sort=threads");
      TEST_CHECK(data.pat_not == true);
      TEST_CHECK(data.use_regex == true);
    }
    folder_hook_data_free(&data);
  }

  // Test: Valid hook with -noregex
  {
    TEST_CASE("Valid hook with -noregex");
    memset(&data, 0, sizeof(data));
    memset(&error, 0, sizeof(error));
    result = parse_folder_hook_line("-noregex work 'set sort=threads'", &data, &error);
    TEST_CHECK(result == true);
    if (result)
    {
      TEST_CHECK_STR_EQ(data.regex, "work");
      TEST_CHECK(data.command != NULL);
      TEST_CHECK(data.pat_not == false);
      TEST_CHECK(data.use_regex == false);
    }
    folder_hook_data_free(&data);
  }

  // Test: -noregex without regex pattern
  {
    TEST_CASE("-noregex without regex");
    memset(&data, 0, sizeof(data));
    memset(&error, 0, sizeof(error));
    result = parse_folder_hook_line("-noregex", &data, &error);
    TEST_CHECK(result == false);
    TEST_CHECK(error.message != NULL);
    folder_hook_data_free(&data);
  }

  // Test: -noregex with regex but no command
  {
    TEST_CASE("-noregex with regex but no command");
    memset(&data, 0, sizeof(data));
    memset(&error, 0, sizeof(error));
    result = parse_folder_hook_line("-noregex pattern", &data, &error);
    TEST_CHECK(result == false);
    TEST_CHECK(error.message != NULL);
    folder_hook_data_free(&data);
  }

  // Test: Complex valid regex
  {
    TEST_CASE("Complex valid regex");
    memset(&data, 0, sizeof(data));
    memset(&error, 0, sizeof(error));
    result = parse_folder_hook_line("^/home/.*inbox$ 'set sort=reverse-date'", &data, &error);
    TEST_CHECK(result == true);
    if (result)
    {
      TEST_CHECK_STR_EQ(data.regex, "^/home/.*inbox$");
      TEST_CHECK_STR_EQ(data.command, "set sort=reverse-date");
    }
    folder_hook_data_free(&data);
  }

  // Test: Hook with quoted regex
  {
    TEST_CASE("Hook with quoted regex");
    memset(&data, 0, sizeof(data));
    memset(&error, 0, sizeof(error));
    result = parse_folder_hook_line("\"mail.*\" 'set sort=threads'", &data, &error);
    TEST_CHECK(result == true);
    if (result)
    {
      TEST_CHECK_STR_EQ(data.regex, "mail.*");
    }
    folder_hook_data_free(&data);
  }

  // Test: Hook with command containing spaces (unquoted uses TOKEN_SPACE)
  {
    TEST_CASE("Hook with command containing spaces");
    memset(&data, 0, sizeof(data));
    memset(&error, 0, sizeof(error));
    result = parse_folder_hook_line(". set sort=date-sent", &data, &error);
    TEST_CHECK(result == true);
    if (result)
    {
      TEST_CHECK_STR_EQ(data.command, "set sort=date-sent");
    }
    folder_hook_data_free(&data);
  }

  // Test: Hook with negation and -noregex
  {
    TEST_CASE("Hook with negation and -noregex");
    memset(&data, 0, sizeof(data));
    memset(&error, 0, sizeof(error));
    result = parse_folder_hook_line("! -noregex inbox 'set sort=threads'", &data, &error);
    TEST_CHECK(result == true);
    if (result)
    {
      TEST_CHECK(data.pat_not == true);
      TEST_CHECK(data.use_regex == false);
    }
    folder_hook_data_free(&data);
  }

  // Test: NULL error pointer on success (should not crash)
  {
    TEST_CASE("NULL error pointer on success");
    memset(&data, 0, sizeof(data));
    result = parse_folder_hook_line(". 'command'", &data, NULL);
    TEST_CHECK(result == true);
    folder_hook_data_free(&data);
  }

  // Test: NULL error pointer on failure (should not crash)
  {
    TEST_CASE("NULL error pointer on failure");
    memset(&data, 0, sizeof(data));
    result = parse_folder_hook_line(NULL, &data, NULL);
    TEST_CHECK(result == false);
    folder_hook_data_free(&data);
  }

  // Test: NULL error pointer on parse error
  {
    TEST_CASE("NULL error pointer on parse error");
    memset(&data, 0, sizeof(data));
    result = parse_folder_hook_line("", &data, NULL);
    TEST_CHECK(result == false);
    folder_hook_data_free(&data);
  }
}
