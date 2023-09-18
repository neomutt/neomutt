/**
 * @file
 * Test code for editor_buffer_get_cursor()
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
#include "editor/lib.h"

void test_editor_buffer_get_cursor(void)
{
  // size_t editor_buffer_get_cursor(struct EnterState *es);

  {
    TEST_CHECK(editor_buffer_get_cursor(NULL) == 0);
  }

  {
    const size_t pos = 8;
    struct EnterState *es = enter_state_new();
    editor_buffer_set(es, "hello world");
    editor_buffer_set_cursor(es, pos);
    TEST_CHECK(editor_buffer_get_cursor(es) == pos);
    enter_state_free(&es);
  }
}
