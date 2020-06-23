/**
 * @file
 * Test code for mutt_extract_message_id()
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
#include <search.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "test_common.h"

int cmp(const void *a, const void *b)
{
  return strcmp(a, *(char **) b);
}

struct TestData
{
  const char *in; // string to parse
  char *out;      // expected result
  size_t len;     // expected parsed len
} test[] = {
  { NULL, NULL, 0 },
  { "apple", NULL, 0 },
  { "foo bar <baz", NULL, 0 },
  { "foo bar > baz", NULL, 0 },
  { "foo bar <foo.bar> boo bar", "<foo.bar>", 17 },
};

void test_mutt_extract_message_id(void)
{
  for (size_t i = 0; i < mutt_array_size(test); i++)
  {
    size_t len = 0;
    char *out = mutt_extract_message_id(test[i].in, &len);
    TEST_CHECK_STR_EQ(test[i].out, out);
    if (!TEST_CHECK(test[i].len == len))
    {
      TEST_MSG("Expected: %zu", test[i].len);
      TEST_MSG("Actual  : %zu", len);
    }
    FREE(&out);
  }

  {
    const char *tokens[] = { "foo bar ", "<foo@bar.baz>", " moo mar", "<moo@mar.maz>" };
    char buf[1024];
    size_t off = 0;
    for (size_t i = 0; i < mutt_array_size(tokens); i++)
    {
      off += mutt_str_copy(&buf[0] + off, tokens[i], sizeof(buf) - off);
    }

    char *tmp = NULL;
    off = 0;
    size_t elems = mutt_array_size(tokens);
    for (const char *it = &buf[0]; (tmp = mutt_extract_message_id(it, &off)); it += off)
    {
      TEST_CHECK(tmp[0] == '<');
      char *found = lfind(tmp, &tokens[0], &elems, sizeof(char *), cmp);
      TEST_CHECK(found != NULL);
      FREE(&tmp);
    }
  }
}
