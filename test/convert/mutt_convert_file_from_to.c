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

void test_mutt_convert_file_from_to(void)
{
  // size_t mutt_convert_file_from_to(FILE *fp, const struct Slist *fromcodes, const struct Slist *tocodes, char **fromcode, char **tocode, struct Content *info);

  {
    /* Conversion from us-ascii to UTF-8. */
    char data[] = "us-ascii text\nline 2 \r\nline3";
    FILE *fp = test_make_file_with_contents(data, sizeof(data) - 1);

    struct Slist *tocodes = slist_parse("utf-8", SLIST_SEP_COLON);
    struct Slist *fromcodes = slist_parse("us-ascii", SLIST_SEP_COLON);
    struct Content info = initial_info;

    char *fromcode = NULL;
    char *tocode = NULL;
    mutt_convert_file_from_to(fp, fromcodes, tocodes, &fromcode, &tocode, &info);

    TEST_CHECK_STR_EQ(fromcode, "us-ascii");
    TEST_MSG("Check failed: %s == us-ascii", fromcode);
    TEST_CHECK_STR_EQ(tocode, "utf-8");
    TEST_MSG("Check failed: %s == utf-8", tocode);

    slist_free(&fromcodes);
    slist_free(&tocodes);
    FREE(&tocode);
    fclose(fp);
  }

  {
    /* Conversion from UTF-8 (in favor of us-ascii) to ISO-8859-2.
     * For reference, the string below translates to the following bytes
     * in ISO-8859-2: '\xf3\xbf\x77\xb3\x00'*/
    char data[] = "line 2\r\nline3\n\xc5\xbc\xc3\xb3\xc5\x82\x77";
    FILE *fp = test_make_file_with_contents(data, sizeof(data) - 1);

    struct Slist *tocodes = slist_parse("iso-8859-2", SLIST_SEP_COLON);
    struct Slist *fromcodes = slist_parse("us-ascii:utf-8", SLIST_SEP_COLON);
    char *fromcode = NULL;
    char *tocode = NULL;
    struct Content info = initial_info;

    mutt_convert_file_from_to(fp, fromcodes, tocodes, &fromcode, &tocode, &info);

    TEST_CHECK_STR_EQ(fromcode, "utf-8");
    TEST_MSG("Check failed: %s == us-ascii", fromcode);
    TEST_CHECK_STR_EQ(tocode, "iso-8859-2");
    TEST_MSG("Check failed: %s == utf-8", tocode);

    slist_free(&fromcodes);
    slist_free(&tocodes);
    FREE(&tocode);
    fclose(fp);
  }
}
