/**
 * @file
 * Test code for mutt_decode_save_attachment()
 *
 * @authors
 * Copyright (C) 2026 Chris Andrew <cjhandrew@gmail.com>
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
#include <unistd.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "attach/lib.h"

/**
 * test_mutt_decode_save_attachment - CID 557729: repeated compose-menu
 * decode-save of the same still-unparsed multipart attachment.
 *
 * mutt_decode_save_attachment() is called with fp == NULL to decode a
 * compose-menu attachment that hasn't been parsed yet. On the first call,
 * b->parts is NULL, so the restore branch (`if (saved_parts)`) is skipped
 * and the freshly-parsed b->parts tree from mutt_parse_part() is left in
 * place. Calling it a second time on the *same* Body (e.g. "save
 * attachment" twice without leaving compose) should exercise the restore
 * branch, since b->parts is now non-NULL - this is the path traced as a
 * leak candidate: the second call's freshly re-parsed tree is discarded
 * without being freed.
 */
void test_mutt_decode_save_attachment(void)
{
  // int mutt_decode_save_attachment(FILE *fp, struct Body *b, const char *path, StateFlags flags, enum SaveAttach opt);

  char attach_path[] = "/tmp/neomutt-test-attach-XXXXXX";
  int fd = mkstemp(attach_path);
  TEST_CHECK(fd >= 0);
  FILE *fp = fdopen(fd, "w");
  TEST_CHECK(fp != NULL);
  fputs("--BOUNDARY\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "part one\r\n"
        "--BOUNDARY--\r\n",
        fp);
  fclose(fp);

  char out1[] = "/tmp/neomutt-test-out1-XXXXXX";
  char out2[] = "/tmp/neomutt-test-out2-XXXXXX";
  TEST_CHECK(mkstemp(out1) >= 0);
  TEST_CHECK(mkstemp(out2) >= 0);

  struct Body body = { 0 };
  body.type = TYPE_MULTIPART;
  body.subtype = mutt_str_dup("mixed");
  body.filename = mutt_str_dup(attach_path);
  mutt_param_set(&body.parameter, "boundary", "BOUNDARY");

  int rc1 = mutt_decode_save_attachment(NULL, &body, out1, STATE_DISPLAY, MUTT_SAVE_NONE);
  TEST_CHECK(rc1 == 0);

  struct Body *parts_after_first_call = body.parts;
  TEST_CHECK(parts_after_first_call != NULL);

  int rc2 = mutt_decode_save_attachment(NULL, &body, out2, STATE_DISPLAY, MUTT_SAVE_NONE);
  TEST_CHECK(rc2 == 0);

  // mutt_decode_save_attachment()'s restore branch sets b->parts back to
  // saved_parts (the pre-call-2 value), so body.parts should be restored
  // to parts_after_first_call - the traced leak candidate is the second
  // call's *intermediate* re-parsed tree, which briefly existed as
  // b->parts during the call but was overwritten by this restore without
  // ever being freed. That intermediate pointer is never exposed back to
  // this test (it's purely internal to the function call), so there's no
  // black-box assertion that can capture it directly - only a leak
  // detector watching the whole process's heap at exit can confirm it.
  TEST_CHECK(body.parts == parts_after_first_call);

  mutt_body_free(&body.parts);
  FREE(&body.subtype);
  FREE(&body.filename);
  mutt_param_free(&body.parameter);

  unlink(attach_path);
  unlink(out1);
  unlink(out2);
}
