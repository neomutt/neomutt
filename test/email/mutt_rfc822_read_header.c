/**
 * @file
 * Test code for mutt_rfc822_read_header()
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "email/lib.h"
#include "test_common.h"

void test_mutt_rfc822_read_header(void)
{
  // struct Envelope *mutt_rfc822_read_header(FILE *fp, struct Email *e, bool user_hdrs, bool weed);

  {
    struct Email e = { 0 };
    TEST_CHECK(!mutt_rfc822_read_header(NULL, &e, false, false));
  }

  {
    FILE *fp = fopen("/dev/null", "r");
    struct Envelope *env = NULL;
    TEST_CHECK((env = mutt_rfc822_read_header(fp, NULL, false, false)) != NULL);
    mutt_env_free(&env);
    fclose(fp);
  }

  {
    char data[] = "From user@host Thu Jan  1 12:00:00 2026\n"
                  "rom user@host Thu Jan  1 12:00:00 2026\n"
                  "Return-Path: <user@example.com>\n"
                  "\n";
    FILE *fp = test_make_file_with_contents(data, sizeof(data) - 1);
    if (!TEST_CHECK(fp != NULL))
      return;

    char buf[128] = { 0 };
    TEST_CHECK(fgets(buf, sizeof(buf), fp) != NULL);
    const long header_offset = strlen(buf);

    struct Email e = { 0 };
    e.offset = 0;
    struct Envelope *env = mutt_rfc822_read_header(fp, &e, false, false);
    TEST_CHECK(env != NULL);
    TEST_CHECK_NUM_EQ(ftello(fp), header_offset);
    TEST_CHECK_NUM_EQ(e.body->offset, header_offset);

    mutt_env_free(&env);
    mutt_body_free(&e.body);
    fclose(fp);
  }
}
