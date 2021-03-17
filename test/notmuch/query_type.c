/**
 * @file
 * Test code for nm_string_to_query_type()
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
#include "notmuch/query.h"

void test_nm_string_to_query_type(void)
{
  // enum NmQueryType nm_string_to_query_type(const char *str);
  MuttLogger = log_disp_null;

  {
    TEST_CHECK(nm_string_to_query_type("threads") == NM_QUERY_TYPE_THREADS);
  }

  {
    TEST_CHECK(nm_string_to_query_type("messages") == NM_QUERY_TYPE_MESGS);
  }

  {
    TEST_CHECK(nm_string_to_query_type("junk") == NM_QUERY_TYPE_MESGS);
  }
}

void test_nm_query_type_to_string(void)
{
  {
    TEST_CHECK(mutt_str_equal(nm_query_type_to_string(NM_QUERY_TYPE_THREADS), "threads"));
  }

  {
    TEST_CHECK(mutt_str_equal(nm_query_type_to_string(NM_QUERY_TYPE_MESGS),
                              "messages"));
  }
}
