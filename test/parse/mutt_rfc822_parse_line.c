/**
 * @file
 * Test code for mutt_rfc822_parse_line()
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
#include "acutest.h"
#include "config.h"
#include "mutt/mutt.h"
#include "address/lib.h"
#include "email/lib.h"

void test_mutt_rfc822_parse_line(void)
{
  // int mutt_rfc822_parse_line(struct Envelope *env, struct Email *e, char *line, char *p, bool user_hdrs, bool weed, bool do_2047);

  {
    struct Email email = { 0 };
    TEST_CHECK(mutt_rfc822_parse_line(NULL, &email, "apple", "banana", false,
                                      false, false) == 0);
  }

  {
    struct Envelope envelope;
    memset(&envelope, 0, sizeof(struct Envelope));
    TEST_CHECK(mutt_rfc822_parse_line(&envelope, NULL, "apple", "banana", false,
                                      false, false) == 0);
  }

  {
    struct Envelope envelope;
    memset(&envelope, 0, sizeof(struct Envelope));
    struct Email email = { 0 };
    TEST_CHECK(mutt_rfc822_parse_line(&envelope, &email, NULL, "banana", false,
                                      false, false) == 0);
  }

  {
    struct Envelope envelope;
    memset(&envelope, 0, sizeof(struct Envelope));
    struct Email email = { 0 };
    TEST_CHECK(mutt_rfc822_parse_line(&envelope, &email, "apple", NULL, false,
                                      false, false) == 0);
  }
}
