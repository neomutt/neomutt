/**
 * @file
 * Test code for editor_case_word()
 *
 * @authors
 * Copyright (C) 2022-2023 Richard Russon <rich@flatcap.org>
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
#include "editor/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

void test_editor_case_word(void)
{
  // int editor_case_word(struct EnterState *es, enum EnterCase ec);

  {
    TEST_CHECK(editor_case_word(NULL, EC_CAPITALIZE) == FR_ERROR);
  }

  {
    struct EnterState *es = enter_state_new();
    TEST_CHECK(editor_case_word(es, EC_CAPITALIZE) == FR_ERROR);
    enter_state_free(&es);
  }

  {
    struct EnterState *es = enter_state_new();
    editor_buffer_set(es, "test string");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 11);
    editor_bol(es);
    TEST_CHECK(editor_case_word(es, EC_CAPITALIZE) == FR_SUCCESS);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 4);

    struct Buffer *buf = buf_pool_get();
    buf_mb_wcstombs(buf, es->wbuf, es->lastchar);
    TEST_CHECK_STR_EQ(buf_string(buf), "Test string");
    buf_pool_release(&buf);
    enter_state_free(&es);
  }

  {
    struct EnterState *es = enter_state_new();
    editor_buffer_set(es, "TEST string");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 11);
    editor_bol(es);
    TEST_CHECK(editor_case_word(es, EC_CAPITALIZE) == FR_SUCCESS);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 4);

    struct Buffer *buf = buf_pool_get();
    buf_mb_wcstombs(buf, es->wbuf, es->lastchar);
    TEST_CHECK_STR_EQ(buf_string(buf), "Test string");
    buf_pool_release(&buf);
    enter_state_free(&es);
  }

  {
    struct EnterState *es = enter_state_new();
    editor_buffer_set(es, "test string");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 11);
    editor_bol(es);
    TEST_CHECK(editor_case_word(es, EC_UPCASE) == FR_SUCCESS);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 4);

    struct Buffer *buf = buf_pool_get();
    buf_mb_wcstombs(buf, es->wbuf, es->lastchar);
    TEST_CHECK_STR_EQ(buf_string(buf), "TEST string");
    buf_pool_release(&buf);
    enter_state_free(&es);
  }

  {
    struct EnterState *es = enter_state_new();
    editor_buffer_set(es, "test string");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 11);
    editor_buffer_set_cursor(es, 7);
    TEST_CHECK(editor_buffer_get_cursor(es) == 7);
    TEST_CHECK(editor_case_word(es, EC_UPCASE) == FR_SUCCESS);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 11);

    struct Buffer *buf = buf_pool_get();
    buf_mb_wcstombs(buf, es->wbuf, es->lastchar);
    TEST_CHECK_STR_EQ(buf_string(buf), "test stRING");
    buf_pool_release(&buf);
    enter_state_free(&es);
  }

  {
    struct EnterState *es = enter_state_new();
    editor_buffer_set(es, "test     string    ");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 19);
    TEST_CHECK(editor_buffer_get_cursor(es) == 19);
    editor_buffer_set_cursor(es, 6);
    TEST_CHECK(editor_buffer_get_cursor(es) == 6);
    TEST_CHECK(editor_case_word(es, EC_UPCASE) == FR_SUCCESS);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 19);
    TEST_CHECK(editor_buffer_get_cursor(es) == 15);

    struct Buffer *buf = buf_pool_get();
    buf_mb_wcstombs(buf, es->wbuf, es->lastchar);
    TEST_CHECK_STR_EQ(buf_string(buf), "test     STRING    ");
    buf_pool_release(&buf);
    enter_state_free(&es);
  }

  {
    struct EnterState *es = enter_state_new();
    editor_buffer_set(es, "test string");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 11);
    editor_bol(es);
    TEST_CHECK(editor_case_word(es, EC_UPCASE) == FR_SUCCESS);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 4);

    struct Buffer *buf = buf_pool_get();
    buf_mb_wcstombs(buf, es->wbuf, es->lastchar);
    TEST_CHECK_STR_EQ(buf_string(buf), "TEST string");
    buf_pool_release(&buf);
    enter_state_free(&es);
  }

  {
    struct EnterState *es = enter_state_new();
    editor_buffer_set(es, "TEST STRING");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 11);
    editor_bol(es);
    TEST_CHECK(editor_case_word(es, EC_DOWNCASE) == FR_SUCCESS);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 4);

    struct Buffer *buf = buf_pool_get();
    buf_mb_wcstombs(buf, es->wbuf, es->lastchar);
    TEST_CHECK_STR_EQ(buf_string(buf), "test STRING");
    buf_pool_release(&buf);
    enter_state_free(&es);
  }
}
