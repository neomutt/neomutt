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
  // char *mutt_ch_choose(const char *fromcode, const char *charsets, const char *u, size_t ulen, char **d, size_t *dlen);

  {
    char buf_in[32] = { 0 };
    char *buf_out = NULL;
    size_t buflen = 0;
    TEST_CHECK(!mutt_ch_choose(NULL, "banana", buf_in, sizeof(buf_in), &buf_out, &buflen));
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
    TEST_CHECK((result = mutt_ch_choose("apple", "banana", NULL, 10, &buf_out, &buflen)) != NULL);
    FREE(&result);
  }

  {
    char buf_in[32] = { 0 };
    size_t buflen = 0;
    TEST_CHECK(!mutt_ch_choose("apple", "banana", buf_in, sizeof(buf_in), NULL, &buflen));
  }

  {
    char buf_in[32] = { 0 };
    char *buf_out = NULL;
    const char *result = NULL;
    TEST_CHECK((result = mutt_ch_choose("apple", "banana", buf_in,
                                        sizeof(buf_in), &buf_out, NULL)) != NULL);
    FREE(&result);
    FREE(&buf_out);
  }
}
