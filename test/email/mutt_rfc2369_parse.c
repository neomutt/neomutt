/**
 * @file
 * Test code for mutt_rfc2369_parse()
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
#include "test_common.h"

void test_mutt_rfc2369_parse(void)
{
  // size_t mutt_rfc2369_parse(const char *body, struct ListHead *links);

  {
    struct ListHead links = { 0 };
    TEST_CHECK_NUM_EQ(mutt_rfc2369_parse(NULL, &links), 0);
    mutt_list_free(&links);
  }

  {
    TEST_CHECK_NUM_EQ(mutt_rfc2369_parse("<mailto:list@example.com>", NULL), 0);
  }

  {
    struct ListHead links = { 0 };
    TEST_CHECK_NUM_EQ(mutt_rfc2369_parse("mailto:list@example.com", &links), 0);
    TEST_CHECK(STAILQ_EMPTY(&links));
    mutt_list_free(&links);
  }

  {
    struct ListHead links = { 0 };
    TEST_CHECK_NUM_EQ(mutt_rfc2369_parse("<mailto:first@example.com>, <https://example.com/list>",
                                         &links),
                      2);

    struct ListNode *np = STAILQ_FIRST(&links);
    TEST_CHECK(np != NULL);
    if (np)
      TEST_CHECK(strcmp(np->data, "mailto:first@example.com") == 0);

    np = np ? STAILQ_NEXT(np, entries) : NULL;
    TEST_CHECK(np != NULL);
    if (np)
      TEST_CHECK(strcmp(np->data, "https://example.com/list") == 0);

    mutt_list_free(&links);
  }
}
