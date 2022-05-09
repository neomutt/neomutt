/**
 * @file
 * Test code for mutt_ch_choose()
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

void test_mutt_ch_choose(void)
{
  // char *mutt_ch_choose(const char *fromcode, const struct Slist *charsets, const char *u, size_t ulen, char **d, size_t *dlen);

  {
    char buf_in[32] = { 0 };
    char *buf_out = NULL;
    size_t buflen = 0;
    struct Slist *charsets = slist_parse("banana", SLIST_SEP_COLON);
    TEST_CHECK(!mutt_ch_choose(NULL, charsets, buf_in, sizeof(buf_in), &buf_out, &buflen));
    slist_free(&charsets);
  }

  {
    char buf_in[32] = { 0 };
    char *buf_out = NULL;
    size_t buflen = 0;
    TEST_CHECK(!mutt_ch_choose("apple", NULL, buf_in, sizeof(buf_in), &buf_out, &buflen));
  }

  {
    char *buf_out = NULL;
    size_t buflen = 0;
    const char *result = NULL;
    struct Slist *charsets = slist_parse("banana", SLIST_SEP_COLON);
    TEST_CHECK((result = mutt_ch_choose("apple", charsets, NULL, 10, &buf_out, &buflen)) != NULL);
    slist_free(&charsets);
    FREE(&result);
  }

  {
    char buf_in[32] = { 0 };
    size_t buflen = 0;
    struct Slist *charsets = slist_parse("banana", SLIST_SEP_COLON);
    TEST_CHECK(!mutt_ch_choose("apple", charsets, buf_in, sizeof(buf_in), NULL, &buflen));
    slist_free(&charsets);
  }

  {
    char buf_in[32] = { 0 };
    char *buf_out = NULL;
    const char *result = NULL;
    struct Slist *charsets = slist_parse("banana", SLIST_SEP_COLON);
    TEST_CHECK((result = mutt_ch_choose("apple", charsets, buf_in,
                                        sizeof(buf_in), &buf_out, NULL)) != NULL);
    slist_free(&charsets);
    FREE(&result);
    FREE(&buf_out);
  }
}
