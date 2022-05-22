/**
 * @file
 * Test code for rfc2231_encode_string()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
#include "email/lib.h"

void test_rfc2231_encode_string(void)
{
  // size_t rfc2231_encode_string(struct ParameterList *head, const char *attribute, char *value);

  {
    struct ParameterList apple = TAILQ_HEAD_INITIALIZER(apple);
    size_t count = rfc2231_encode_string(&apple, NULL, "apple");
    TEST_CHECK(count == 0);
    TEST_CHECK(TAILQ_EMPTY(&apple));
  }

  {
    struct ParameterList banana = TAILQ_HEAD_INITIALIZER(banana);
    size_t count = rfc2231_encode_string(&banana, "banana", NULL);
    TEST_CHECK(count == 0);
    TEST_CHECK(TAILQ_EMPTY(&banana));
  }
}
