/**
 * @file
 * Test code for mutt_ch_convert_string()
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
#include <stddef.h>
#include <stdbool.h>
#include "email/lib.h"
#include "convert/lib.h"
#include "convert_common.h"

bool content_equal(struct Content const *lhs, struct Content const *rhs)
{
  return lhs->hibin == rhs->hibin && lhs->lobin == rhs->lobin &&
         lhs->nulbin == rhs->nulbin && lhs->crlf == rhs->crlf &&
         lhs->ascii == rhs->ascii && lhs->linemax == rhs->linemax &&
         lhs->space == rhs->space && lhs->binary == rhs->binary &&
         lhs->from == rhs->from && lhs->dot == rhs->dot && lhs->cr == rhs->cr;
}

void test_mutt_update_content_info(void)
{
  // void mutt_update_content_info(struct Content *info, struct ContentState *s, char *buf, size_t buflen);

  struct ContentState initial_state = {
    .dot = false, .from = false, .linelen = 0, .was_cr = false, .whitespace = 0
  };

  {
    /* Check that if buf is NULL and the last character was CR,
     * content is set as binary, and no changes are made to the state. */
    struct Content info = initial_info;
    struct ContentState state = initial_state;
    state.was_cr = true;
    mutt_update_content_info(&info, &state, NULL, 0);
    TEST_CHECK(info.binary);
    TEST_MSG("Check failed: %d == 1", info.binary);
    /* Everything else is the same. */
    info.binary = false;
    TEST_CHECK(content_equal(&info, &initial_info));
  }

  {
    /* Check that if buf is NULL and the last character was not CR,
     * nothing is updated unless linelen is greater than linemax. */
    struct Content info = initial_info;
    struct ContentState state = initial_state;
    mutt_update_content_info(&info, &state, NULL, 0);
    TEST_CHECK(content_equal(&info, &initial_info));

    state.linelen = 1;
    mutt_update_content_info(&info, &state, NULL, 0);
    TEST_CHECK(info.linemax == 1);
    TEST_MSG("Check failed: %d == 1", info.linemax);
    /* Everything else is the same. */
    info.linemax = 0;
    TEST_CHECK(content_equal(&info, &initial_info));
  }

  {
    /* Check that if there is a \r but not followed by \n, then it's binary. */
    struct Content info = initial_info;
    struct ContentState state = initial_state;
    state.was_cr = true;
    mutt_update_content_info(&info, &state, "abc\rabc", 7);
    TEST_CHECK(info.binary);
    TEST_MSG("Check failed: %d == 1", info.binary);

    info = initial_info;
    state = initial_state;
    state.was_cr = true;
    mutt_update_content_info(&info, &state, "abc", 3);
    TEST_CHECK(info.binary);
    TEST_MSG("Check failed: %d == 1", info.binary);

    info = initial_info;
    state = initial_state;
    mutt_update_content_info(&info, &state, "abc", 3);
    TEST_CHECK(!info.binary);
    TEST_MSG("Check failed: %d == 0", info.binary);
  }

  {
    /* Check that the longest line is recorded. */
    struct Content info = initial_info;
    struct ContentState state = initial_state;
    info.linemax = 7;

    mutt_update_content_info(&info, &state, "abc\nabc\nx\nqwerty", 16);
    TEST_CHECK(info.linemax == 7);
    TEST_MSG("Check failed: %d == 7", info.linemax);

    state = initial_state;
    mutt_update_content_info(&info, &state, "abc\nasdfghjkl\nx\nqwerty", 22);
    TEST_CHECK(info.linemax == 10);
    TEST_MSG("Check failed: %d == 10", info.linemax);

    /* Check that the character count carries over to the next call. */
    mutt_update_content_info(&info, &state, "abcdef\na", 8);
    TEST_CHECK(info.linemax == 13);
    TEST_MSG("Check failed: %d == 13", info.linemax);
  }

  {
    /* Check line consisting of only a dot. */
    struct Content info = initial_info;
    struct ContentState state = initial_state;

    mutt_update_content_info(&info, &state, "abc\n.\nx\nqwerty", 14);
    TEST_CHECK(info.dot);
    TEST_MSG("Check failed: %d == 1", info.dot);

    info = initial_info;
    state = initial_state;
    state.dot = true;
    mutt_update_content_info(&info, &state, "\naaa", 4);
    TEST_CHECK(info.dot);
    TEST_MSG("Check failed: %d == 1", info.dot);

    info = initial_info;
    state = initial_state;
    mutt_update_content_info(&info, &state, "abc\r\n.\nx\nqwerty", 14);
    TEST_CHECK(info.dot);
    TEST_MSG("Check failed: %d == 1", info.dot);

    info = initial_info;
    state = initial_state;
    mutt_update_content_info(&info, &state, "abc\nx\nqwerty", 12);
    TEST_CHECK(!info.dot);
    TEST_MSG("Check failed: %d == 0", info.dot);
  }

  {
    /* Check has CR. */
    struct Content info = initial_info;
    struct ContentState state = initial_state;

    mutt_update_content_info(&info, &state, "abc\rabc", 7);
    TEST_CHECK(info.cr);
    TEST_MSG("Check failed: %d == 1", info.cr);

    info = initial_info;
    state = initial_state;
    mutt_update_content_info(&info, &state, "abcabc", 6);
    TEST_CHECK(!info.cr);
    TEST_MSG("Check failed: %d == 0", info.cr);
  }

  {
    /* Check that has CRLF. */
    struct Content info = initial_info;
    struct ContentState state = initial_state;

    mutt_update_content_info(&info, &state, "abc\r\nabc", 8);
    TEST_CHECK(info.crlf == 1);
    TEST_MSG("Check failed: %d == 1", info.crlf);

    info = initial_info;
    state = initial_state;
    mutt_update_content_info(&info, &state, "abc\nabc", 7);
    TEST_CHECK(info.crlf == 1);
    TEST_MSG("Check failed: %d == 1", info.crlf);

    info = initial_info;
    state = initial_state;
    mutt_update_content_info(&info, &state, "abcabc", 6);
    TEST_CHECK(info.crlf == 0);
    TEST_MSG("Check failed: %d == 0", info.crlf);

    // TODO
    // info = initial_info;
    // state = initial_state;
    // state.was_cr = true;
    // mutt_update_content_info(&info, &state, "\nabc", 4);
    // TEST_CHECK(info.crlf == 1);
  }

  {
    /* Check starts with From. */
    struct Content info = initial_info;
    struct ContentState state = initial_state;

    mutt_update_content_info(&info, &state, "\nFrom \n", 7);
    TEST_CHECK(info.from);
    TEST_MSG("Check failed: %d == 1", info.from);

    info = initial_info;
    state = initial_state;
    mutt_update_content_info(&info, &state, "blah\nFr", 7);
    TEST_CHECK(!info.from);
    TEST_MSG("Check failed: %d == 0", info.from);
    mutt_update_content_info(&info, &state, "om \ns", 5);
    TEST_CHECK(info.from);
    TEST_MSG("Check failed: %d == 1", info.from);
  }

  {
    /* Check starts with From. */
    struct Content info = initial_info;
    struct ContentState state = initial_state;

    mutt_update_content_info(&info, &state, "\nFrom \n", 7);
    TEST_CHECK(info.from);

    info = initial_info;
    state = initial_state;
    mutt_update_content_info(&info, &state, "blah\nFr", 7);
    TEST_CHECK(!info.from);
    TEST_MSG("Check failed: %d == 0", info.from);
    mutt_update_content_info(&info, &state, "om \ns", 5);
    TEST_CHECK(info.from);
    TEST_MSG("Check failed: %d == 1", info.from);
  }

  {
    /* Check whitespace at end of lines. */
    struct Content info = initial_info;
    struct ContentState state = initial_state;

    mutt_update_content_info(&info, &state, "x\nFrom \n", 8);
    TEST_CHECK(info.space);
    TEST_MSG("Check failed: %d == 1", info.space);

    info = initial_info;
    state = initial_state;
    mutt_update_content_info(&info, &state, "blah\nFr", 7);
    TEST_CHECK(!info.space);
    TEST_MSG("Check failed: %d == 0", info.space);
    mutt_update_content_info(&info, &state, "om \ns", 5);
    TEST_CHECK(info.space);
    TEST_MSG("Check failed: %d == 1", info.space);

    info = initial_info;
    state = initial_state;
    mutt_update_content_info(&info, &state, "blah\nFrom ", 10);
    TEST_CHECK(!info.space);
    TEST_MSG("Check failed: %d == 0", info.space);
    mutt_update_content_info(&info, &state, "\ns", 2);
    TEST_CHECK(info.space);
    TEST_MSG("Check failed: %d == 1", info.space);
  }

  {
    /* Check count ASCII. */
    struct Content info = initial_info;
    struct ContentState state = initial_state;
    info.ascii = 2;
    mutt_update_content_info(&info, &state, "qwertyżółw", 13);
    TEST_CHECK(info.ascii == 9);
    TEST_MSG("Check failed: %d == 9", info.ascii);
  }

  {
    /* Check count null characters. */
    struct Content info = initial_info;
    struct ContentState state = initial_state;
    info.nulbin = 2;
    mutt_update_content_info(&info, &state, "\0qwerty\0\0w\0", 11);
    TEST_CHECK(info.nulbin == 6);
    TEST_MSG("Check failed: %d == 6", info.nulbin);
  }

  {
    /* Check count null characters. */
    struct Content info = initial_info;
    struct ContentState state = initial_state;
    info.hibin = 2;
    info.lobin = 3;
    mutt_update_content_info(&info, &state, "\0żółw\0\0w\0\r\n", 14);
    TEST_CHECK(info.hibin == 8);
    TEST_MSG("Check failed: %d == 9", info.hibin);
    TEST_CHECK(info.lobin == 7);
    TEST_MSG("Check failed: %d == 10", info.lobin);
  }
}
