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
  struct Hook *hook = NULL;

  // Test: NULL line
  {
    TEST_CASE("NULL line");
    error.message = NULL;
    error.position = 0;
    hook = parse_folder_hook_line(NULL, &error);
    TEST_CHECK(hook == NULL);
    TEST_CHECK(error.message != NULL);
    TEST_CHECK(error.position == 0);
  }

  // Test: Empty line
  {
    TEST_CASE("Empty line");
    error.message = NULL;
    error.position = 0;
    hook = parse_folder_hook_line("", &error);
    TEST_CHECK(hook == NULL);
    TEST_CHECK(error.message != NULL);
  }

  // Test: Only whitespace
  {
    TEST_CASE("Only whitespace");
    error.message = NULL;
    error.position = 0;
    hook = parse_folder_hook_line("   ", &error);
    TEST_CHECK(hook == NULL);
    TEST_CHECK(error.message != NULL);
  }

  // Test: Only regex, no command
  {
    TEST_CASE("Only regex, no command");
    error.message = NULL;
    error.position = 0;
    hook = parse_folder_hook_line(".", &error);
    TEST_CHECK(hook == NULL);
    TEST_CHECK(error.message != NULL);
  }

  // Test: Valid simple hook
  {
    TEST_CASE("Valid simple hook");
    error.message = NULL;
    error.position = 0;
    hook = parse_folder_hook_line(". 'set sort=date-sent'", &error);
    TEST_CHECK(hook != NULL);
    if (hook)
    {
      TEST_CHECK(hook->id == CMD_FOLDER_HOOK);
      TEST_CHECK_STR_EQ(hook->regex.pattern, ".");
      TEST_CHECK_STR_EQ(hook->command, "set sort=date-sent");
      TEST_CHECK(hook->regex.pat_not == false);
      TEST_CHECK(hook->regex.regex != NULL);
      hook_free(&hook);
    }
  }

  // Test: Valid hook with negation
  {
    TEST_CASE("Valid hook with negation");
    error.message = NULL;
    error.position = 0;
    hook = parse_folder_hook_line("! . 'set sort=threads'", &error);
    TEST_CHECK(hook != NULL);
    if (hook)
    {
      TEST_CHECK(hook->id == CMD_FOLDER_HOOK);
      TEST_CHECK_STR_EQ(hook->regex.pattern, ".");
      TEST_CHECK_STR_EQ(hook->command, "set sort=threads");
      TEST_CHECK(hook->regex.pat_not == true);
      hook_free(&hook);
    }
  }

  // Test: Valid hook with -noregex
  {
    TEST_CASE("Valid hook with -noregex");
    error.message = NULL;
    error.position = 0;
    hook = parse_folder_hook_line("-noregex work 'set sort=threads'", &error);
    TEST_CHECK(hook != NULL);
    if (hook)
    {
      TEST_CHECK(hook->id == CMD_FOLDER_HOOK);
      // -noregex causes the pattern to be sanitized
      TEST_CHECK(hook->command != NULL);
      TEST_CHECK(hook->regex.pat_not == false);
      hook_free(&hook);
    }
  }

  // Test: -noregex without regex pattern
  {
    TEST_CASE("-noregex without regex");
    error.message = NULL;
    error.position = 0;
    hook = parse_folder_hook_line("-noregex", &error);
    TEST_CHECK(hook == NULL);
    TEST_CHECK(error.message != NULL);
  }

  // Test: -noregex with regex but no command
  {
    TEST_CASE("-noregex with regex but no command");
    error.message = NULL;
    error.position = 0;
    hook = parse_folder_hook_line("-noregex pattern", &error);
    TEST_CHECK(hook == NULL);
    TEST_CHECK(error.message != NULL);
  }

  // Test: Invalid regex pattern
  {
    TEST_CASE("Invalid regex pattern");
    error.message = NULL;
    error.position = 0;
    hook = parse_folder_hook_line("[unclosed 'command'", &error);
    TEST_CHECK(hook == NULL);
    TEST_CHECK(error.message != NULL);
  }

  // Test: Complex valid regex
  {
    TEST_CASE("Complex valid regex");
    error.message = NULL;
    error.position = 0;
    hook = parse_folder_hook_line("^/home/.*inbox$ 'set sort=reverse-date'", &error);
    TEST_CHECK(hook != NULL);
    if (hook)
    {
      TEST_CHECK_STR_EQ(hook->regex.pattern, "^/home/.*inbox$");
      TEST_CHECK_STR_EQ(hook->command, "set sort=reverse-date");
      hook_free(&hook);
    }
  }

  // Test: Hook with quoted regex
  {
    TEST_CASE("Hook with quoted regex");
    error.message = NULL;
    error.position = 0;
    hook = parse_folder_hook_line("\"mail.*\" 'set sort=threads'", &error);
    TEST_CHECK(hook != NULL);
    if (hook)
    {
      TEST_CHECK_STR_EQ(hook->regex.pattern, "mail.*");
      hook_free(&hook);
    }
  }

  // Test: Hook with command containing spaces (unquoted uses TOKEN_SPACE)
  {
    TEST_CASE("Hook with command containing spaces");
    error.message = NULL;
    error.position = 0;
    hook = parse_folder_hook_line(". set sort=date-sent", &error);
    TEST_CHECK(hook != NULL);
    if (hook)
    {
      TEST_CHECK_STR_EQ(hook->command, "set sort=date-sent");
      hook_free(&hook);
    }
  }

  // Test: Hook with negation and -noregex
  {
    TEST_CASE("Hook with negation and -noregex");
    error.message = NULL;
    error.position = 0;
    hook = parse_folder_hook_line("! -noregex inbox 'set sort=threads'", &error);
    TEST_CHECK(hook != NULL);
    if (hook)
    {
      TEST_CHECK(hook->regex.pat_not == true);
      hook_free(&hook);
    }
  }

  // Test: NULL error pointer (should not crash)
  {
    TEST_CASE("NULL error pointer on success");
    hook = parse_folder_hook_line(". 'command'", NULL);
    TEST_CHECK(hook != NULL);
    if (hook)
    {
      hook_free(&hook);
    }
  }

  // Test: NULL error pointer on failure (should not crash)
  {
    TEST_CASE("NULL error pointer on failure");
    hook = parse_folder_hook_line(NULL, NULL);
    TEST_CHECK(hook == NULL);
  }

  // Test: NULL error pointer on parse error
  {
    TEST_CASE("NULL error pointer on parse error");
    hook = parse_folder_hook_line("", NULL);
    TEST_CHECK(hook == NULL);
  }
}
