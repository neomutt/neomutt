/**
 * @file
 * Test code for editor_kill_word()
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
#include "email/lib.h"
#include "core/lib.h"
#include "enter/lib.h"

void test_editor_kill_word(void)
{
  // int editor_kill_word(struct EnterState *es);

  {
    TEST_CHECK(editor_kill_word(NULL) == FR_ERROR);
  }

  {
    struct EnterState *es = mutt_enter_state_new();
    TEST_CHECK(editor_kill_word(es) == FR_ERROR);
    mutt_enter_state_free(&es);
  }

  {
    struct EnterState *es = mutt_enter_state_new();
    editor_buffer_set(es, "test string");
    editor_buffer_set_cursor(es, 0);
    TEST_CHECK(editor_buffer_get_cursor(es) == 0);
    TEST_CHECK(editor_kill_word(es) == FR_ERROR);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 0);
    mutt_enter_state_free(&es);
  }

  {
    struct EnterState *es = mutt_enter_state_new();
    editor_buffer_set(es, "test string");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 11);
    TEST_CHECK(editor_kill_word(es) == FR_SUCCESS);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 5);
    TEST_CHECK(editor_buffer_get_cursor(es) == 5);
    mutt_enter_state_free(&es);
  }

  {
    struct EnterState *es = mutt_enter_state_new();
    editor_buffer_set(es, "test string--");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 13);
    TEST_CHECK(editor_buffer_get_cursor(es) == 13);
    TEST_CHECK(editor_kill_word(es) == FR_SUCCESS);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 12);
    TEST_CHECK(editor_buffer_get_cursor(es) == 12);
    mutt_enter_state_free(&es);
  }

  {
    struct EnterState *es = mutt_enter_state_new();
    editor_buffer_set(es, "apple 义勇军 banana");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 16);
    TEST_CHECK(editor_buffer_get_cursor(es) == 16);
    editor_buffer_set_cursor(es, 10);
    TEST_CHECK(editor_buffer_get_cursor(es) == 10);
    TEST_CHECK(editor_kill_word(es) == FR_SUCCESS);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 12);
    TEST_CHECK(editor_buffer_get_cursor(es) == 6);
    mutt_enter_state_free(&es);
  }

  {
    struct EnterState *es = mutt_enter_state_new();
    editor_buffer_set(es, "I ❤️xyz abc");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 11);
    editor_buffer_set_cursor(es, 7);
    TEST_CHECK(editor_buffer_get_cursor(es) == 7);
    TEST_CHECK(editor_kill_word(es) == FR_SUCCESS);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 8);
    TEST_CHECK(editor_buffer_get_cursor(es) == 4);
    mutt_enter_state_free(&es);
  }
}
