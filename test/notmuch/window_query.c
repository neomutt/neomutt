/**
 * @file
 * Test code for notmuch windowed queries
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

void test_nm_windowed_query_from_query(void)
{
  // Ensure legacy-behavior functions as expected with duration = 0
  {
    char buf[1024] = "\0";
    enum NmWindowQueryRc rc =
        nm_windowed_query_from_query(buf, 1024, false, 0, 0, "tag:inbox", "month");
    TEST_CHECK(rc == NM_WINDOW_QUERY_INVALID_DURATION);
  }

  // Check legacy behavior with duration > 0
  {
    char buf[1024] = "\0";
    enum NmWindowQueryRc rc =
        nm_windowed_query_from_query(buf, 1024, false, 1, 0, "tag:inbox", "month");
    TEST_CHECK(rc == NM_WINDOW_QUERY_SUCCESS);
    TEST_CHECK(mutt_str_equal(buf, "date:1month.. and tag:inbox"));
  }

  // Check duration 1 one position back
  {
    char buf[1024] = "\0";
    enum NmWindowQueryRc rc =
        nm_windowed_query_from_query(buf, 1024, false, 1, 1, "tag:inbox", "month");
    TEST_CHECK(rc == NM_WINDOW_QUERY_SUCCESS);
    TEST_CHECK(mutt_str_equal(buf, "date:2month..1month and tag:inbox"));
  }

  // Check duration 1 span 3 positions back
  {
    char buf[1024] = "\0";
    enum NmWindowQueryRc rc =
        nm_windowed_query_from_query(buf, 1024, false, 1, 3, "tag:inbox", "month");
    TEST_CHECK(rc == NM_WINDOW_QUERY_SUCCESS);
    TEST_CHECK(mutt_str_equal(buf, "date:4month..3month and tag:inbox"));
  }

  // Check 3 duration span 3 position backs
  {
    char buf[1024] = "\0";
    enum NmWindowQueryRc rc =
        nm_windowed_query_from_query(buf, 1024, false, 3, 3, "tag:inbox", "month");
    TEST_CHECK(rc == NM_WINDOW_QUERY_SUCCESS);
    TEST_CHECK(mutt_str_equal(buf, "date:12month..9month and tag:inbox"));
  }

  // Check invalid timebase
  {
    char buf[1024] = "\0";
    enum NmWindowQueryRc rc =
        nm_windowed_query_from_query(buf, 1024, false, 3, 3, "tag:inbox", "months");
    TEST_CHECK(rc == NM_WINDOW_QUERY_INVALID_TIMEBASE);
  }

  // Check zero-duration support with force_enable
  {
    char buf[1024] = "\0";
    enum NmWindowQueryRc rc =
        nm_windowed_query_from_query(buf, 1024, true, 0, 0, "tag:inbox", "month");
    TEST_CHECK(rc == NM_WINDOW_QUERY_SUCCESS);
    TEST_CHECK(mutt_str_equal(buf, "date:0month.. and tag:inbox"));
  }

  // Check zero duration span 1 position back
  {
    char buf[1024] = "\0";
    enum NmWindowQueryRc rc =
        nm_windowed_query_from_query(buf, 1024, true, 0, 1, "tag:inbox", "month");
    TEST_CHECK(rc == NM_WINDOW_QUERY_SUCCESS);
    TEST_CHECK(mutt_str_equal(buf, "date:1month..1month and tag:inbox"));
  }

  // Check zero duration span 3 position backs
  {
    char buf[1024] = "\0";
    enum NmWindowQueryRc rc =
        nm_windowed_query_from_query(buf, 1024, true, 0, 3, "tag:inbox", "month");
    TEST_CHECK(rc == NM_WINDOW_QUERY_SUCCESS);
    TEST_CHECK(mutt_str_equal(buf, "date:3month..3month and tag:inbox"));
  }
}
