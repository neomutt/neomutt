/**
 * @file
 * Test code for mutt_rfc2369_first_mailto()
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
#include "email/lib.h"

void test_mutt_rfc2369_first_mailto(void)
{
  // char *mutt_rfc2369_first_mailto(const char *body);

  {
    char *mailto = mutt_rfc2369_first_mailto(NULL);
    TEST_CHECK(mailto == NULL);
    FREE(&mailto);
  }

  {
    char *mailto = mutt_rfc2369_first_mailto("mailto:list@example.com");
    TEST_CHECK(mailto == NULL);
    FREE(&mailto);
  }

  {
    char *mailto = mutt_rfc2369_first_mailto("<https://example.com/list>, <mailto:list@example.com>");
    TEST_CHECK(mailto != NULL);
    TEST_CHECK(strcmp(mailto, "mailto:list@example.com") == 0);
    FREE(&mailto);
  }

  {
    char *mailto = mutt_rfc2369_first_mailto("<mailto:first@example.com>, <mailto:second@example.com>");
    TEST_CHECK(mailto != NULL);
    TEST_CHECK(strcmp(mailto, "mailto:first@example.com") == 0);
    FREE(&mailto);
  }
}
