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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "convert/lib.h"
#include "test_common.h"

static struct ConfigDef CharsetVars[] = {
  // clang-format off
  { "config_charset", DT_STRING, 0, 0, charset_validator, },
  { "send_charset", DT_SLIST|SLIST_SEP_COLON|SLIST_ALLOW_EMPTY|DT_CHARSET_STRICT, IP "us-ascii:iso-8859-1:utf-8", 0, charset_slist_validator, },
  { NULL },
  // clang-format on
};

void test_mutt_get_content_info(void)
{
  // struct Content *mutt_get_content_info(const char *fname, struct Body *b, struct ConfigSubset *sub);

  static const char *text = "file\ncontent";

  struct Buffer *fname = buf_pool_get();
  buf_mktemp(fname);

  FILE *fp = fopen(buf_string(fname), "w");
  TEST_CHECK(fp != NULL);
  TEST_MSG("unable to open temp file for writing");

  TEST_CHECK(fwrite(text, strlen(text), 1, fp) > 0);
  TEST_MSG("unable to write to temp file: %s", buf_string(fname));
  fclose(fp);

  struct ConfigSubset *sub = NeoMutt->sub;
  struct ConfigSet *cs = sub->cs;
  TEST_CHECK(cs_register_variables(cs, CharsetVars, DT_NO_FLAGS));

  struct Body *body = mutt_body_new();
  struct Content *content = mutt_get_content_info(buf_string(fname), body, sub);
  TEST_CHECK(content != NULL);

  TEST_CHECK(content->hibin == 0);
  TEST_MSG("Check failed: %d == 0", content->hibin);
  TEST_CHECK(content->lobin == 0);
  TEST_MSG("Check failed: %d == 0", content->lobin);
  TEST_CHECK(content->nulbin == 0);
  TEST_MSG("Check failed: %d == 0", content->nulbin);
  TEST_CHECK(content->crlf == 1);
  TEST_MSG("Check failed: %d == 1", content->crlf);
  TEST_CHECK(content->ascii == 11);
  TEST_MSG("Check failed: %d == 11", content->ascii);
  TEST_CHECK(content->linemax == 7);
  TEST_MSG("Check failed: %d == 7", content->linemax);
  TEST_CHECK(!content->space);
  TEST_MSG("Check failed: %d == 0", content->space);
  TEST_CHECK(!content->binary);
  TEST_MSG("Check failed: %d == 0", content->binary);
  TEST_CHECK(!content->from);
  TEST_MSG("Check failed: %d == 0", content->from);
  TEST_CHECK(!content->dot);
  TEST_MSG("Check failed: %d == 0", content->dot);
  TEST_CHECK(content->cr == 0);
  TEST_MSG("Check failed: %d == 0", content->cr);

  buf_pool_release(&fname);
  mutt_body_free(&body);
  FREE(&content);
}
