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
#include "mutt/lib.h"
#include "email/lib.h"
#include "convert/lib.h"
#include "convert_common.h"
#include "test_common.h"

void test_mutt_convert_file_to(void)
{
  // static size_t convert_file_to(FILE *fp, const char *fromcode, struct Slist const *const tocodes, int *tocode, struct Content *info)

  {
    /* Conversion from us-ascii to UTF-8. */
    char data[] = "us-ascii text\nline 2 \r\nline3";
    FILE *fp = test_make_file_with_contents(data, sizeof(data) - 1);

    struct Slist *tocodes = slist_parse("utf-8", SLIST_SEP_COLON);
    int tocode = 0;
    struct Content info = initial_info;

    size_t rc = mutt_convert_file_to(fp, "us-ascii", tocodes, &tocode, &info);
    TEST_CHECK(rc == 0);
    TEST_MSG("Check failed: %d == 0", rc);

    TEST_CHECK(tocode == 0);
    TEST_MSG("Check failed: %d == 0", tocode);
    TEST_CHECK(info.hibin == 0);
    TEST_MSG("Check failed: %d == 0", info.hibin);
    TEST_CHECK(info.lobin == 0);
    TEST_MSG("Check failed: %d == 0", info.lobin);
    TEST_CHECK(info.nulbin == 0);
    TEST_MSG("Check failed: %d == 0", info.nulbin);
    TEST_CHECK(info.crlf == 2);
    TEST_MSG("Check failed: %d == 2", info.crlf);
    TEST_CHECK(info.ascii == 25);
    TEST_MSG("Check failed: %d == 25", info.ascii);
    TEST_CHECK(info.linemax == 14);
    TEST_MSG("Check failed: %d == 13", info.linemax);
    TEST_CHECK(info.space);
    TEST_MSG("Check failed: %d == 1", info.space);
    TEST_CHECK(!info.binary);
    TEST_MSG("Check failed: %d == 0", info.binary);
    TEST_CHECK(!info.from);
    TEST_MSG("Check failed: %d == 0", info.from);
    TEST_CHECK(!info.dot);
    TEST_MSG("Check failed: %d == 0", info.dot);
    TEST_CHECK(info.cr == 1);
    TEST_MSG("Check failed: %d == 1", info.cr);

    slist_free(&tocodes);
    fclose(fp);
  }

  {
    /* Conversion from ISO-8859-2 to us-ascii, despite invalid characters,
     * because us-ascii is the only tocode. */
    char data[] = "line 2\r\nline3\n\xf3\xbf\x77\xb3\x00";
    FILE *fp = test_make_file_with_contents(data, sizeof(data) - 1);

    struct Slist *tocodes = slist_parse("us-ascii", SLIST_SEP_COLON);
    int tocode = 0;
    struct Content info = initial_info;

    size_t rc = mutt_convert_file_to(fp, "iso-8859-2", tocodes, &tocode, &info);
    TEST_CHECK(rc != sizeof(data) - 1);

    slist_free(&tocodes);
    fclose(fp);
  }

  {
    /* Conversion from ISO-8859-2 to us-ascii or ISO-8859-1.
     * Neither is a valid conversion, so rc == -1. */
    char data[] = "line 2\r\nline3\n\xf3\xbf\x77\xb3\x00";
    FILE *fp = test_make_file_with_contents(data, sizeof(data) - 1);

    struct Slist *tocodes = slist_parse("us-ascii:iso-8859-1", SLIST_SEP_COLON);
    int tocode = 0;
    struct Content info = initial_info;

    size_t rc = mutt_convert_file_to(fp, "iso-8859-2", tocodes, &tocode, &info);
    TEST_CHECK(rc != sizeof(data) - 1);

    slist_free(&tocodes);
    fclose(fp);
  }

  {
    /* Conversion from ISO-8859-2 to us-ascii or ISO-8859-1,
     * but with all valid characters. */
    char data[] = "line 2\r\nline3\n";
    FILE *fp = test_make_file_with_contents(data, sizeof(data) - 1);

    struct Slist *tocodes = slist_parse("us-ascii:iso-8859-1", SLIST_SEP_COLON);
    int tocode = 0;
    struct Content info = initial_info;

    size_t rc = mutt_convert_file_to(fp, "iso-8859-2", tocodes, &tocode, &info);
    TEST_CHECK(rc == 0);
    TEST_MSG("Check failed: %d == 0", rc);

    TEST_CHECK(tocode == 0);
    TEST_MSG("Check failed: %d == 0", tocode);

    slist_free(&tocodes);
    fclose(fp);
  }

  {
    /* Conversion from ISO-8859-2 to UTF-8 in favor of us-ascii.
     * For reference, the string below translates to the following bytes
     * in UTF-8: '\xc5\xbc\xc3\xb3\xc5\x82\x77'*/
    char data[] = "line 2\r\nline3\n\xf3\xbf\x77\xb3\x00";
    FILE *fp = test_make_file_with_contents(data, sizeof(data) - 1);

    struct Slist *tocodes = slist_parse("us-ascii:utf-8", SLIST_SEP_COLON);
    int tocode = 0;
    struct Content info = initial_info;

    size_t rc = mutt_convert_file_to(fp, "iso-8859-2", tocodes, &tocode, &info);

    TEST_CHECK(tocode == 1);
    TEST_MSG("Check failed: %d == 1", tocode);

    /* Special case for converting to UTF-8. */
    TEST_CHECK(rc == 0);
    TEST_MSG("Check failed: %d == 0", rc);

    TEST_CHECK(info.hibin == 6);
    TEST_MSG("Check failed: %d == 6", info.hibin);
    TEST_CHECK(info.lobin == 1);
    TEST_MSG("Check failed: %d == 1", info.lobin);
    TEST_CHECK(info.nulbin == 1);
    TEST_MSG("Check failed: %d == 1", info.nulbin);
    TEST_CHECK(info.crlf == 2);
    TEST_MSG("Check failed: %d == 2", info.crlf);
    TEST_CHECK(info.ascii == 12);
    TEST_MSG("Check failed: %d == 12", info.ascii);
    TEST_CHECK(info.linemax == 8);
    TEST_MSG("Check failed: %d == 8", info.linemax);
    TEST_CHECK(!info.space);
    TEST_MSG("Check failed: %d == 0", info.space);
    TEST_CHECK(!info.binary);
    TEST_MSG("Check failed: %d == 0", info.binary);
    TEST_CHECK(!info.from);
    TEST_MSG("Check failed: %d == 0", info.from);
    TEST_CHECK(!info.dot);
    TEST_MSG("Check failed: %d == 0", info.dot);
    TEST_CHECK(info.cr == 1);
    TEST_MSG("Check failed: %d == 1", info.cr);

    slist_free(&tocodes);
    fclose(fp);
  }
}
