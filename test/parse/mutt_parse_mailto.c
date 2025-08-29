/**
 * @file
 * Test code for mutt_parse_mailto()
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019-2023 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2022 Claes Nästén <pekdon@gmail.com>
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
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "test_common.h"

static void check_addrlist(struct AddressList *list, const char *const exp[], size_t num)
{
  struct Buffer *parsed = buf_pool_get();
  if (mutt_addrlist_write(list, parsed, false) == 0)
  {
    TEST_MSG("Expected: parsed %s (...)", exp[0]);
    TEST_MSG("Actual  : not parsed");
  }

  char *pp = buf_strdup(parsed);
  char *orig = pp;
  buf_pool_release(&parsed);
  for (size_t i = 0; i < num; ++i)
  {
    char *tok = mutt_str_skip_whitespace(mutt_str_sep(&pp, ","));
    TEST_CHECK_STR_EQ(tok, exp[i]);
  }
  FREE(&orig);
}

void test_mutt_parse_mailto(void)
{
  // int mutt_parse_mailto(struct Envelope *e, char **body, const char *src);

  mutt_list_insert_head(&MailToAllow, "cc");
  mutt_list_insert_head(&MailToAllow, "body");

  {
    char *body = NULL;
    TEST_CHECK(!mutt_parse_mailto(NULL, &body, "apple"));
  }

  {
    struct Envelope envelope;
    memset(&envelope, 0, sizeof(struct Envelope));
    TEST_CHECK(!mutt_parse_mailto(&envelope, NULL, "apple"));
  }

  {
    struct Envelope envelope;
    memset(&envelope, 0, sizeof(struct Envelope));
    char *body = NULL;
    TEST_CHECK(!mutt_parse_mailto(&envelope, &body, NULL));
  }

  {
    struct Envelope *env = mutt_env_new();
    const char *to[] = { "mail@example.com" };
    const char *cc[] = { "foo@bar.baz", "joo@jar.jaz" };
    const char *body = "Some text - it should be pct-encoded";
    const char *body_enc = "Some%20text%20-%20it%20should%20be%20pct-encoded";
    char mailto[1024];
    snprintf(mailto, sizeof(mailto), "mailto:%s?cc=%s,%s&body=%s", to[0], cc[0],
             cc[1], body_enc);
    char *parsed_body = NULL;
    if (!TEST_CHECK(mutt_parse_mailto(env, &parsed_body, mailto)))
    {
      TEST_MSG("Expected: parsed <%s>", mailto);
      TEST_MSG("Actual  : NULL");
    }
    check_addrlist(&env->to, to, mutt_array_size(to));
    check_addrlist(&env->cc, cc, mutt_array_size(cc));
    TEST_CHECK_STR_EQ(parsed_body, body);
    FREE(&parsed_body);
    mutt_env_free(&env);
  }
}
