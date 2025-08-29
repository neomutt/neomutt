/**
 * @file
 * Test code for notmuch tag functions
 *
 * @authors
 * Copyright (C) 2021 Austin Ray <austin@austinray.io>
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Pietro Cerutti <gahr@gahr.ch>
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
#include "notmuch/tag.h" // IWYU pragma: keep
#include "notmuch/lib.h"
#include "test_common.h"

void test_nm_tag_string_to_tags(void)
{
  // struct NmTags nm_tag_str_to_tags(const char *tag_str);

  {
    const char *input = "inbox,archive";
    struct NmTags output = nm_tag_str_to_tags(input);

    TEST_CHECK(ARRAY_SIZE(&output.tags) == 2);

    TEST_CHECK_STR_EQ(*ARRAY_GET(&output.tags, 0), "inbox");
    TEST_CHECK_STR_EQ(*ARRAY_GET(&output.tags, 1), "archive");

    nm_tag_array_free(&output);
  }

  {
    const char *input = "inbox archive";
    struct NmTags output = nm_tag_str_to_tags(input);

    TEST_CHECK(ARRAY_SIZE(&output.tags) == 2);

    TEST_CHECK_STR_EQ(*ARRAY_GET(&output.tags, 0), "inbox");
    TEST_CHECK_STR_EQ(*ARRAY_GET(&output.tags, 1), "archive");

    nm_tag_array_free(&output);
  }

  {
    const char *input = "inbox archive,sent";
    struct NmTags output = nm_tag_str_to_tags(input);

    TEST_CHECK(ARRAY_SIZE(&output.tags) == 3);

    TEST_CHECK_STR_EQ(*ARRAY_GET(&output.tags, 0), "inbox");
    TEST_CHECK_STR_EQ(*ARRAY_GET(&output.tags, 1), "archive");
    TEST_CHECK_STR_EQ(*ARRAY_GET(&output.tags, 2), "sent");

    nm_tag_array_free(&output);
  }

  {
    const char *input = "inbox,,archive";
    struct NmTags output = nm_tag_str_to_tags(input);

    TEST_CHECK(ARRAY_SIZE(&output.tags) == 1);

    TEST_CHECK_STR_EQ(*ARRAY_GET(&output.tags, 0), "inbox");

    nm_tag_array_free(&output);
  }

  {
    const char *input = "inbox, ,archive";
    struct NmTags output = nm_tag_str_to_tags(input);

    TEST_CHECK(ARRAY_SIZE(&output.tags) == 1);

    TEST_CHECK_STR_EQ(*ARRAY_GET(&output.tags, 0), "inbox");

    nm_tag_array_free(&output);
  }
}
