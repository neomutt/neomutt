/**
 * @file
 * Test code for parse context structures
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
#include <string.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "parse/lib.h"
#include "test_common.h"

/**
 * test_file_location - Test FileLocation functions
 */
static void test_file_location(void)
{
  // Test file_location_init
  {
    struct FileLocation fl = { 0 };
    file_location_init(&fl, "/path/to/file.rc", 42);

    TEST_CHECK(fl.filename != NULL);
    TEST_CHECK_STR_EQ(fl.filename, "/path/to/file.rc");
    TEST_CHECK_NUM_EQ(fl.lineno, 42);

    file_location_clear(&fl);
    TEST_CHECK(fl.filename == NULL);
    TEST_CHECK_NUM_EQ(fl.lineno, 0);
  }

  // Test NULL handling
  {
    file_location_init(NULL, "/path/to/file.rc", 1);
    file_location_clear(NULL);
    // Should not crash
    TEST_CHECK(true);
  }
}

/**
 * test_parse_context - Test ParseContext functions
 */
static void test_parse_context(void)
{
  // Test basic init/free
  {
    struct ParseContext *pc = parse_context_new();
    parse_context_init(pc, CO_CONFIG_FILE);

    TEST_CHECK(ARRAY_EMPTY(&pc->locations));
    TEST_CHECK_NUM_EQ(pc->origin, CO_CONFIG_FILE);
    TEST_CHECK_NUM_EQ(pc->hook_id, CMD_NONE);

    parse_context_free(&pc);
  }

  // Test push/pop operations
  {
    struct ParseContext *pc = parse_context_new();
    parse_context_init(pc, CO_CONFIG_FILE);

    // Push first file
    parse_context_push(pc, "/path/to/first.rc", 10);
    TEST_CHECK_NUM_EQ(ARRAY_SIZE(&pc->locations), 1);

    struct FileLocation *fl = parse_context_current(pc);
    TEST_CHECK(fl != NULL);
    TEST_CHECK_STR_EQ(fl->filename, "/path/to/first.rc");
    TEST_CHECK_NUM_EQ(fl->lineno, 10);

    // Push second file (nested source)
    parse_context_push(pc, "/path/to/second.rc", 20);
    TEST_CHECK_NUM_EQ(ARRAY_SIZE(&pc->locations), 2);

    fl = parse_context_current(pc);
    TEST_CHECK(fl != NULL);
    TEST_CHECK_STR_EQ(fl->filename, "/path/to/second.rc");
    TEST_CHECK_NUM_EQ(fl->lineno, 20);

    // Pop second file
    parse_context_pop(pc);
    TEST_CHECK_NUM_EQ(ARRAY_SIZE(&pc->locations), 1);

    fl = parse_context_current(pc);
    TEST_CHECK(fl != NULL);
    TEST_CHECK_STR_EQ(fl->filename, "/path/to/first.rc");

    // Pop first file
    parse_context_pop(pc);
    TEST_CHECK_NUM_EQ(ARRAY_SIZE(&pc->locations), 0);

    fl = parse_context_current(pc);
    TEST_CHECK(fl == NULL);

    // Pop from empty stack should not crash
    parse_context_pop(pc);
    TEST_CHECK(ARRAY_EMPTY(&pc->locations));

    parse_context_free(&pc);
  }

  // Test parse_context_contains for cyclic detection
  {
    struct ParseContext *pc = parse_context_new();
    parse_context_init(pc, CO_CONFIG_FILE);

    parse_context_push(pc, "/path/to/first.rc", 1);
    parse_context_push(pc, "/path/to/second.rc", 1);
    parse_context_push(pc, "/path/to/third.rc", 1);

    TEST_CHECK(parse_context_contains(pc, "/path/to/first.rc"));
    TEST_CHECK(parse_context_contains(pc, "/path/to/second.rc"));
    TEST_CHECK(parse_context_contains(pc, "/path/to/third.rc"));
    TEST_CHECK(!parse_context_contains(pc, "/path/to/fourth.rc"));
    TEST_CHECK(!parse_context_contains(pc, NULL));
    TEST_CHECK(!parse_context_contains(NULL, "/path/to/first.rc"));

    parse_context_free(&pc);
  }

  // Test parse_context_cwd
  {
    struct ParseContext *pc = parse_context_new();
    parse_context_init(pc, CO_CONFIG_FILE);

    TEST_CHECK(parse_context_cwd(pc) == NULL);

    parse_context_push(pc, "/path/to/config.rc", 1);
    const char *cwd = parse_context_cwd(pc);
    TEST_CHECK(cwd != NULL);
    TEST_CHECK_STR_EQ(cwd, "/path/to/config.rc");

    parse_context_free(&pc);
  }

  // Test NULL handling
  {
    parse_context_init(NULL, CO_CONFIG_FILE);
    parse_context_free(NULL);
    parse_context_push(NULL, "/path", 1);
    parse_context_pop(NULL);
    TEST_CHECK(parse_context_current(NULL) == NULL);
    // Should not crash
    TEST_CHECK(true);
  }

  // Test different origins
  {
    struct ParseContext *pc = parse_context_new();

    parse_context_init(pc, CO_USER);
    TEST_CHECK_NUM_EQ(pc->origin, CO_USER);
    parse_context_free(&pc);

    parse_context_init(pc, CO_HOOK);
    TEST_CHECK_NUM_EQ(pc->origin, CO_HOOK);
    pc->hook_id = CMD_FOLDER_HOOK;
    TEST_CHECK_NUM_EQ(pc->hook_id, CMD_FOLDER_HOOK);
    parse_context_free(&pc);

    parse_context_init(pc, CO_LUA);
    TEST_CHECK_NUM_EQ(pc->origin, CO_LUA);
    parse_context_free(&pc);
  }
}

/**
 * test_parse_error - Test ParseError functions
 */
static void test_parse_error(void)
{
  // Test basic init/free
  {
    struct ParseError *pe = parse_error_new();
    parse_error_init(pe);

    TEST_CHECK(buf_is_empty(pe->message));
    TEST_CHECK(pe->filename == NULL);
    TEST_CHECK_NUM_EQ(pe->lineno, 0);
    TEST_CHECK_NUM_EQ(pe->origin, CO_CONFIG_FILE);
    TEST_CHECK_NUM_EQ(pe->result, MUTT_CMD_SUCCESS);

    parse_error_free(&pe);
  }

  // Test setting error information
  {
    struct ParseError *pe = parse_error_new();
    parse_error_init(pe);

    parse_error_set(pe, MUTT_CMD_ERROR, "/path/to/file.rc", 42,
                           "Error: %s not found", "variable");

    TEST_CHECK_STR_EQ(buf_string(pe->message), "Error: variable not found");
    TEST_CHECK_STR_EQ(pe->filename, "/path/to/file.rc");
    TEST_CHECK_NUM_EQ(pe->lineno, 42);
    TEST_CHECK_NUM_EQ(pe->result, MUTT_CMD_ERROR);

    // Update error information
    parse_error_set(pe, MUTT_CMD_WARNING, "/another/file.rc", 100,
                           "Warning: %s deprecated", "option");

    TEST_CHECK_STR_EQ(buf_string(pe->message), "Warning: option deprecated");
    TEST_CHECK_STR_EQ(pe->filename, "/another/file.rc");
    TEST_CHECK_NUM_EQ(pe->lineno, 100);
    TEST_CHECK_NUM_EQ(pe->result, MUTT_CMD_WARNING);

    parse_error_free(&pe);
  }

  // Test NULL handling
  {
    parse_error_init(NULL);
    parse_error_free(NULL);
    parse_error_set(NULL, MUTT_CMD_ERROR, "/path", 1, "error");
    // Should not crash
    TEST_CHECK(true);
  }

  // Test NULL filename
  {
    struct ParseError *pe = parse_error_new();
    parse_error_init(pe);

    parse_error_set(pe, MUTT_CMD_ERROR, NULL, 0, "Error message");

    TEST_CHECK_STR_EQ(buf_string(pe->message), "Error message");
    TEST_CHECK(pe->filename == NULL);
    TEST_CHECK_NUM_EQ(pe->lineno, 0);

    parse_error_free(&pe);
  }
}

void test_parse_pcontext(void)
{
  TEST_CASE("FileLocation");
  test_file_location();

  TEST_CASE("ParseContext");
  test_parse_context();

  TEST_CASE("ParseError");
  test_parse_error();
}
