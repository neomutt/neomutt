/**
 * @file
 * Test code for email custom_priority field
 *
 * @authors
 * Copyright (C) 2024 GitHub Copilot
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
#include "email/lib.h"

void test_email_custom_priority(void)
{
  // Test that custom_priority field is properly initialized
  {
    struct Email *e = email_new();
    TEST_CHECK(e != NULL);
    TEST_CHECK(e->custom_priority == 0);
    email_free(&e);
    TEST_CHECK(e == NULL);
  }

  // Test that custom_priority can be set and read
  {
    struct Email *e = email_new();
    TEST_CHECK(e != NULL);
    e->custom_priority = 42;
    TEST_CHECK(e->custom_priority == 42);
    e->custom_priority = -10;
    TEST_CHECK(e->custom_priority == -10);
    email_free(&e);
  }
}
