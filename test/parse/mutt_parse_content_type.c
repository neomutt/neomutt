/**
 * @file
 * Test code for mutt_parse_content_type()
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
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"

void test_mutt_parse_content_type(void)
{
  // void mutt_parse_content_type(const char *s, struct Body *ct);

  {
    struct Body body = { 0 };
    mutt_parse_content_type(NULL, &body);
    TEST_CHECK_(1, "mutt_parse_content_type(NULL, &body)");
  }

  {
    mutt_parse_content_type("apple", NULL);
    TEST_CHECK_(1, "mutt_parse_content_type(\"apple\", NULL)");
  }
}
