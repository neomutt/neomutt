/**
 * @file
 * Test code for notmuch tag functions
 *
 * @authors
 * Copyright (C) 2021 Austin Ray <austin@austinray.io>
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
#include "notmuch/tag.h"

void test_nm_tag_string_to_tags(void)
{
  // struct TagArray nm_tag_str_to_tags(const char *tag_str);

  {
    const char *input = "inbox,archive";
    struct TagArray output = nm_tag_str_to_tags(input);

    TEST_CHECK(mutt_str_equal("inbox", *ARRAY_GET(&output.tags, 0)));
    TEST_CHECK(mutt_str_equal("archive", *ARRAY_GET(&output.tags, 1)));

    nm_tag_array_free(&output);
  }

  {
    const char *input = "inbox archive";
    struct TagArray output = nm_tag_str_to_tags(input);

    TEST_CHECK(mutt_str_equal("inbox", *ARRAY_GET(&output.tags, 0)));
    TEST_CHECK(mutt_str_equal("archive", *ARRAY_GET(&output.tags, 1)));

    nm_tag_array_free(&output);
  }

  {
    const char *input = "inbox archive,sent";
    struct TagArray output = nm_tag_str_to_tags(input);

    TEST_CHECK(mutt_str_equal("inbox", *ARRAY_GET(&output.tags, 0)));
    TEST_CHECK(mutt_str_equal("archive", *ARRAY_GET(&output.tags, 1)));
    TEST_CHECK(mutt_str_equal("sent", *ARRAY_GET(&output.tags, 2)));

    nm_tag_array_free(&output);
  }
}
