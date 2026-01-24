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
#include <string.h>
#include "mutt/lib.h"
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

    file_location_free(&fl);
    TEST_CHECK(fl.filename == NULL);
    TEST_CHECK_NUM_EQ(fl.lineno, 0);
  }

  // Test NULL handling
  {
    file_location_init(NULL, "/path/to/file.rc", 1);
    file_location_free(NULL);
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
    struct ParseContext pctx = { 0 };
    parse_context_init(&pctx, CO_CONFIG_FILE);

    TEST_CHECK(ARRAY_EMPTY(&pctx.locations));
    TEST_CHECK_NUM_EQ(pctx.origin, CO_CONFIG_FILE);
    TEST_CHECK_NUM_EQ(pctx.hook_id, CMD_NONE);

    parse_context_free(&pctx);
    TEST_CHECK(ARRAY_EMPTY(&pctx.locations));
  }

  // Test push/pop operations
  {
    struct ParseContext pctx = { 0 };
    parse_context_init(&pctx, CO_CONFIG_FILE);

    // Push first file
    parse_context_push(&pctx, "/path/to/first.rc", 10);
    TEST_CHECK_NUM_EQ(ARRAY_SIZE(&pctx.locations), 1);

    struct FileLocation *fl = parse_context_current(&pctx);
    TEST_CHECK(fl != NULL);
    TEST_CHECK_STR_EQ(fl->filename, "/path/to/first.rc");
    TEST_CHECK_NUM_EQ(fl->lineno, 10);

    // Push second file (nested source)
    parse_context_push(&pctx, "/path/to/second.rc", 20);
    TEST_CHECK_NUM_EQ(ARRAY_SIZE(&pctx.locations), 2);

    fl = parse_context_current(&pctx);
    TEST_CHECK(fl != NULL);
    TEST_CHECK_STR_EQ(fl->filename, "/path/to/second.rc");
    TEST_CHECK_NUM_EQ(fl->lineno, 20);

    // Pop second file
    parse_context_pop(&pctx);
    TEST_CHECK_NUM_EQ(ARRAY_SIZE(&pctx.locations), 1);

    fl = parse_context_current(&pctx);
    TEST_CHECK(fl != NULL);
    TEST_CHECK_STR_EQ(fl->filename, "/path/to/first.rc");

    // Pop first file
    parse_context_pop(&pctx);
    TEST_CHECK_NUM_EQ(ARRAY_SIZE(&pctx.locations), 0);

    fl = parse_context_current(&pctx);
    TEST_CHECK(fl == NULL);

    // Pop from empty stack should not crash
    parse_context_pop(&pctx);
    TEST_CHECK(ARRAY_EMPTY(&pctx.locations));

    parse_context_free(&pctx);
  }

  // Test parse_context_contains for cyclic detection
  {
    struct ParseContext pctx = { 0 };
    parse_context_init(&pctx, CO_CONFIG_FILE);

    parse_context_push(&pctx, "/path/to/first.rc", 1);
    parse_context_push(&pctx, "/path/to/second.rc", 1);
    parse_context_push(&pctx, "/path/to/third.rc", 1);

    TEST_CHECK(parse_context_contains(&pctx, "/path/to/first.rc"));
    TEST_CHECK(parse_context_contains(&pctx, "/path/to/second.rc"));
    TEST_CHECK(parse_context_contains(&pctx, "/path/to/third.rc"));
    TEST_CHECK(!parse_context_contains(&pctx, "/path/to/fourth.rc"));
    TEST_CHECK(!parse_context_contains(&pctx, NULL));
    TEST_CHECK(!parse_context_contains(NULL, "/path/to/first.rc"));

    parse_context_free(&pctx);
  }

  // Test parse_context_cwd
  {
    struct ParseContext pctx = { 0 };
    parse_context_init(&pctx, CO_CONFIG_FILE);

    TEST_CHECK(parse_context_cwd(&pctx) == NULL);

    parse_context_push(&pctx, "/path/to/config.rc", 1);
    const char *cwd = parse_context_cwd(&pctx);
    TEST_CHECK(cwd != NULL);
    TEST_CHECK_STR_EQ(cwd, "/path/to/config.rc");

    parse_context_free(&pctx);
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
    struct ParseContext pctx = { 0 };

    parse_context_init(&pctx, CO_USER);
    TEST_CHECK_NUM_EQ(pctx.origin, CO_USER);
    parse_context_free(&pctx);

    parse_context_init(&pctx, CO_HOOK);
    TEST_CHECK_NUM_EQ(pctx.origin, CO_HOOK);
    pctx.hook_id = CMD_FOLDER_HOOK;
    TEST_CHECK_NUM_EQ(pctx.hook_id, CMD_FOLDER_HOOK);
    parse_context_free(&pctx);

    parse_context_init(&pctx, CO_LUA);
    TEST_CHECK_NUM_EQ(pctx.origin, CO_LUA);
    parse_context_free(&pctx);
  }
}

/**
 * test_config_parse_error - Test ConfigParseError functions
 */
static void test_config_parse_error(void)
{
  // Test basic init/free
  {
    struct ConfigParseError err = { 0 };
    config_parse_error_init(&err);

    TEST_CHECK(buf_is_empty(&err.message));
    TEST_CHECK(err.filename == NULL);
    TEST_CHECK_NUM_EQ(err.lineno, 0);
    TEST_CHECK_NUM_EQ(err.origin, CO_CONFIG_FILE);
    TEST_CHECK_NUM_EQ(err.result, MUTT_CMD_SUCCESS);

    config_parse_error_free(&err);
  }

  // Test setting error information
  {
    struct ConfigParseError err = { 0 };
    config_parse_error_init(&err);

    config_parse_error_set(&err, MUTT_CMD_ERROR, "/path/to/file.rc", 42,
                           "Error: %s not found", "variable");

    TEST_CHECK_STR_EQ(buf_string(&err.message), "Error: variable not found");
    TEST_CHECK_STR_EQ(err.filename, "/path/to/file.rc");
    TEST_CHECK_NUM_EQ(err.lineno, 42);
    TEST_CHECK_NUM_EQ(err.result, MUTT_CMD_ERROR);

    // Update error information
    config_parse_error_set(&err, MUTT_CMD_WARNING, "/another/file.rc", 100,
                           "Warning: %s deprecated", "option");

    TEST_CHECK_STR_EQ(buf_string(&err.message), "Warning: option deprecated");
    TEST_CHECK_STR_EQ(err.filename, "/another/file.rc");
    TEST_CHECK_NUM_EQ(err.lineno, 100);
    TEST_CHECK_NUM_EQ(err.result, MUTT_CMD_WARNING);

    config_parse_error_free(&err);
    TEST_CHECK(err.filename == NULL);
  }

  // Test NULL handling
  {
    config_parse_error_init(NULL);
    config_parse_error_free(NULL);
    config_parse_error_set(NULL, MUTT_CMD_ERROR, "/path", 1, "error");
    // Should not crash
    TEST_CHECK(true);
  }

  // Test NULL filename
  {
    struct ConfigParseError err = { 0 };
    config_parse_error_init(&err);

    config_parse_error_set(&err, MUTT_CMD_ERROR, NULL, 0, "Error message");

    TEST_CHECK_STR_EQ(buf_string(&err.message), "Error message");
    TEST_CHECK(err.filename == NULL);
    TEST_CHECK_NUM_EQ(err.lineno, 0);

    config_parse_error_free(&err);
  }
}

void test_parse_pcontext(void)
{
  TEST_CASE("FileLocation");
  test_file_location();

  TEST_CASE("ParseContext");
  test_parse_context();

  TEST_CASE("ConfigParseError");
  test_config_parse_error();
}
