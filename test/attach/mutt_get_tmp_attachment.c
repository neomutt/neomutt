/**
 * @file
 * Test code for mutt_get_tmp_attachment()
 *
 * @authors
 * Copyright (C) 2026 Dennis Schön <mail@dennis-schoen.de>
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
#include <unistd.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "attach/lib.h"
#include "test_common.h" // IWYU pragma: keep

/**
 * test_mutt_get_tmp_attachment - Regression test for issue #990
 *
 * Two attachments from different messages share the same decoded filename
 * (simulating e.g. two emails each carrying "invoice.pdf").  Each call to
 * mutt_get_tmp_attachment() must produce an independent temp file so that the
 * first attachment's content is not clobbered by the second.  The temp file
 * must also preserve the original file extension so that viewers can infer the
 * MIME type from the filename.
 *
 * Before the fix, mailcap_expand_filename() derived a deterministic path from
 * the attachment filename, meaning both calls resolved to the same path in
 * tmp_dir and the second write silently overwrote the first.
 */
void test_mutt_get_tmp_attachment(void)
{
  // int mutt_get_tmp_attachment(struct Body *b);

  /* Use a name with a known extension to simulate two different messages both
   * carrying e.g. "invoice.pdf".  mkstemp gives us a unique base path for
   * this run; we then append ".pdf" so the extension is present, matching
   * what a real decoded attachment path looks like. */
  char src_base[] = "/tmp/neomutt-test-invoice-XXXXXX";
  int fd = mkstemp(src_base);
  TEST_CHECK(fd >= 0);
  close(fd);

  /* Build the .pdf path alongside the mkstemp base. */
  char src[256] = { 0 };
  snprintf(src, sizeof(src), "%s.pdf", src_base);
  unlink(src_base);

  /* Write content A — represents the first message's attachment. */
  FILE *fp = fopen(src, "w");
  TEST_CHECK(fp != NULL);
  if (fp)
  {
    fputs("content-one\n", fp);
    fclose(fp);
  }

  struct Body b1 = { 0 };
  b1.type = TYPE_APPLICATION;
  b1.subtype = mutt_str_dup("pdf");
  b1.filename = mutt_str_dup(src);

  int rc1 = mutt_get_tmp_attachment(&b1);
  TEST_CHECK(rc1 == 0);
  TEST_CHECK(b1.unlink == true);

  /* Temp file must carry the .pdf suffix so viewers open it correctly. */
  const char *ext1 = strrchr(b1.filename, '.');
  TEST_CHECK(ext1 != NULL);
  if (ext1)
    TEST_CHECK_STR_EQ(ext1, ".pdf");

  /* Overwrite the source path with content B — represents a second message's
   * attachment decoded to the same filename. */
  fp = fopen(src, "w");
  TEST_CHECK(fp != NULL);
  if (fp)
  {
    fputs("content-two\n", fp);
    fclose(fp);
  }

  struct Body b2 = { 0 };
  b2.type = TYPE_APPLICATION;
  b2.subtype = mutt_str_dup("pdf");
  b2.filename = mutt_str_dup(src);

  int rc2 = mutt_get_tmp_attachment(&b2);
  TEST_CHECK(rc2 == 0);
  TEST_CHECK(b2.unlink == true);

  /* The two temp paths must be distinct — no collision. */
  TEST_CHECK(!mutt_str_equal(b1.filename, b2.filename));

  /* b1's temp file must still hold content A, not be clobbered by b2. */
  char buf1[64] = { 0 };
  FILE *fp1 = fopen(b1.filename, "r");
  TEST_CHECK(fp1 != NULL);
  if (fp1)
  {
    TEST_CHECK(fgets(buf1, sizeof(buf1), fp1) != NULL);
    fclose(fp1);
  }
  TEST_CHECK_STR_EQ(buf1, "content-one\n");

  /* b2's temp file must hold content B. */
  char buf2[64] = { 0 };
  FILE *fp2 = fopen(b2.filename, "r");
  TEST_CHECK(fp2 != NULL);
  if (fp2)
  {
    TEST_CHECK(fgets(buf2, sizeof(buf2), fp2) != NULL);
    fclose(fp2);
  }
  TEST_CHECK_STR_EQ(buf2, "content-two\n");

  /* cleanup */
  unlink(b1.filename);
  unlink(b2.filename);
  unlink(src);
  FREE(&b1.filename);
  FREE(&b1.subtype);
  FREE(&b2.filename);
  FREE(&b2.subtype);
}
