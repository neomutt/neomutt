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
#include "mutt/lib.h"
#include "email/lib.h"

static void check_header(const struct ListHead *actual, const char *expected[], size_t num_expected)
{
  size_t num_actual = 0;
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, actual, entries)
  {
    TEST_CHECK(num_actual < num_expected);
    if (num_actual < num_expected)
      TEST_CHECK(strcmp(np->data, expected[num_actual]) == 0);
    num_actual++;
  }
  TEST_CHECK(num_actual == num_expected);
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

  const char *archive[] = {
    "https://example.com/archive",
    "mailto:archive@example.com",
  };
  check_header(&headers.archive, archive, countof(archive));

  const char *help[] = { "mailto:help@example.com" };
  check_header(&headers.help, help, countof(help));

  const char *owner[] = { "mailto:owner@example.com" };
  check_header(&headers.owner, owner, countof(owner));

  check_header(&headers.post, NULL, 0);

  const char *subscribe[] = {
    "https://example.com/subscribe",
    "mailto:subscribe@example.com",
  };
  check_header(&headers.subscribe, subscribe, countof(subscribe));

  const char *unsubscribe[] = { "mailto:unsubscribe@example.com" };
  check_header(&headers.unsubscribe, unsubscribe, countof(unsubscribe));

  mutt_rfc2369_list_headers_free(&headers);
  fclose(fp);
}
