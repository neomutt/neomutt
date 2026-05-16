/**
 * @file
 * Test code for mutt_rfc2369_read_headers()
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
#include <stdio.h>
#include <string.h>
#include "email/lib.h"

static void check_header(const char *actual, const char *expected)
{
  if (!expected)
  {
    TEST_CHECK(actual == NULL);
    return;
  }

  TEST_CHECK(actual != NULL);
  if (actual)
    TEST_CHECK(strcmp(actual, expected) == 0);
}

void test_mutt_rfc2369_read_headers(void)
{
  // void mutt_rfc2369_read_headers(FILE *fp, struct Rfc2369ListHeaders *headers);

  struct Rfc2369ListHeaders headers = { 0 };

  mutt_rfc2369_read_headers(NULL, &headers);
  mutt_rfc2369_read_headers(stdin, NULL);

  FILE *fp = tmpfile();
  if (!TEST_CHECK(fp != NULL))
    return;

  fputs("From list@example.com Sat Jan 01 00:00:00 2000\n", fp);
  fputs("List-Archive: <https://example.com/archive>,\n", fp);
  fputs("\t<mailto:archive@example.com>\n", fp);
  fputs("List-Help: <mailto:help@example.com>\n", fp);
  fputs("List-Owner: <mailto:owner@example.com>\n", fp);
  fputs("List-Post: NO\n", fp);
  fputs("List-Subscribe: <https://example.com/subscribe>,\n", fp);
  fputs("\t<mailto:subscribe@example.com>\n", fp);
  fputs("List-Unsubscribe: <mailto:unsubscribe@example.com>\n", fp);
  fputs("\n", fp);
  rewind(fp);

  mutt_rfc2369_read_headers(fp, &headers);

  check_header(headers.archive, "mailto:archive@example.com");
  check_header(headers.help, "mailto:help@example.com");
  check_header(headers.owner, "mailto:owner@example.com");
  check_header(headers.post, NULL);
  check_header(headers.subscribe, "mailto:subscribe@example.com");
  check_header(headers.unsubscribe, "mailto:unsubscribe@example.com");

  mutt_rfc2369_list_headers_free(&headers);
  fclose(fp);
}
