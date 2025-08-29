/**
 * @file
 * Test code for config cache
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
#include "config/common.h" // IWYU pragma: keep
#include "config/lib.h"
#include "core/lib.h"
#include "test_common.h" // IWYU pragma: keep

void test_config_cache(void)
{
  log_line(__func__);

  struct ConfigSubset *sub = NeoMutt->sub;

  {
    const struct Slist *c_assumed_charset = cc_assumed_charset();
    TEST_CHECK(c_assumed_charset == NULL);
    int rc = cs_subset_str_string_set(sub, "assumed_charset", "us-ascii:utf-8", NULL);
    TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS);
    config_cache_cleanup();
  }

  {
    const char *c_charset = cc_charset();
    TEST_CHECK(c_charset != NULL);
    int rc = cs_subset_str_string_set(sub, "charset", "us-ascii", NULL);
    TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS);
    config_cache_cleanup();
  }

  {
    const char *c_maildir_field_delimiter = cc_maildir_field_delimiter();
    TEST_CHECK(c_maildir_field_delimiter != NULL);
    int rc = cs_subset_str_string_set(sub, "maildir_field_delimiter", ";", NULL);
    TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS);
    config_cache_cleanup();
  }

  log_line(__func__);
}
