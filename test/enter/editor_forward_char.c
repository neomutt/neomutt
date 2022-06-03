/**
 * @file
 * Test code for editor_forward_char()
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

void test_editor_forward_char(void)
{
  // int editor_forward_char(struct EnterState *es);

  {
    TEST_CHECK(editor_forward_char(NULL) == FR_ERROR);
  }

  {
    struct EnterState *es = mutt_enter_state_new();
    TEST_CHECK(editor_forward_char(es) == FR_ERROR);
    mutt_enter_state_free(&es);
  }

  {
    struct EnterState *es = mutt_enter_state_new();
    editor_buffer_set(es, "test string");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 11);
    TEST_CHECK(editor_forward_char(es) == FR_ERROR);
    mutt_enter_state_free(&es);
  }

  {
    struct EnterState *es = mutt_enter_state_new();
    editor_buffer_set(es, "test string");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 11);
    editor_buffer_set_cursor(es, 3);
    TEST_CHECK(editor_forward_char(es) == FR_SUCCESS);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 11);
    TEST_CHECK(editor_buffer_get_cursor(es) == 4);
    mutt_enter_state_free(&es);
  }

  {
    struct EnterState *es = mutt_enter_state_new();
    editor_buffer_set(es, "义勇军");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 3);
    editor_buffer_set_cursor(es, 1);
    TEST_CHECK(editor_buffer_get_cursor(es) == 1);
    TEST_CHECK(editor_forward_char(es) == FR_SUCCESS);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 3);
    TEST_CHECK(editor_buffer_get_cursor(es) == 2);
    mutt_enter_state_free(&es);
  }

  {
    struct EnterState *es = mutt_enter_state_new();
    editor_buffer_set(es, "I ❤️xyz");
    TEST_CHECK(editor_buffer_get_lastchar(es) == 7);
    TEST_CHECK(editor_buffer_get_cursor(es) == 7);
    editor_buffer_set_cursor(es, 2);
    TEST_CHECK(editor_buffer_get_cursor(es) == 2);
    TEST_CHECK(editor_forward_char(es) == FR_SUCCESS);
    TEST_CHECK(editor_buffer_get_lastchar(es) == 7);
    TEST_CHECK(editor_buffer_get_cursor(es) == 4);
    mutt_enter_state_free(&es);
  }
}
