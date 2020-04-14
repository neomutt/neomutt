/**
 * @file
 * Test code for mutt_date_parse_date()
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
#include "mutt/lib.h"

struct ParseDateTest
{
  const char *str;
  time_t expected;
};

void test_mutt_date_parse_date(void)
{
  // time_t mutt_date_parse_date(const char *s, struct Tz *tz_out);

  {
    struct Tz tz;
    memset(&tz, 0, sizeof(tz));
    TEST_CHECK(mutt_date_parse_date(NULL, &tz) != 0);
  }

  {
    TEST_CHECK(mutt_date_parse_date("apple", NULL) != 0);
  }

  // clang-format off
  struct ParseDateTest parse_tests[] = {
     // [ weekday , ] day-of-month month year hour:minute:second [ timezone ]
     { "Wed, 13 Jun 2007 12:34:56 +0100",            1181734496 },
     { "Wed, 13 Jun 2007 12:34:56 MET DST",          1181734496 },
     { "Wed, 13 Jun 20 12:34:56 +0100",              1592048096 },
     { "Wed, 13 Jun 2007 12:34 +0100",               1181734440 },
     { "Wed, 13 (06) Jun 2007 (seven) 12:34 +0100",  1181734440 },
     { "Wed, 13 Jun 2007 12:34:56 -0100",            1181741696 },
     { "Wed, 13 Jun (Ju (06)n) 2007 12:34:56 -0100", 1181741696 },
     { "Wed, 13 Jun 2007 12:34:56 -0100 (CET)",      1181741696 },
     { "Wed, 13 Jun 2007 12:34:56 -0100 (CET)",      1181741696 },
     { "Wed, 13 Jun 2007 12:34:56 +0000 (FOO)",      1181738096 },
     { "Wed, 13 Jun 2007 12:34:56 UTC",              1181738096 },
     { "Tue, 07 Apr 2020 15:06:31 GMT",              1586271991 },
     { "Tue,  7 Apr 2020 15:06:31 GMT",              1586271991 },
     { "Tue, 7 Apr 2020 15:06:31 GMT",               1586271991 },
     { "13 Jun 2007 12:34:56",                       1181738096 },

     /* Single-digit day / hour */
     { "Fri, 10 Jul 2009 3:22:08 -0400",             1247210528 }, 
     { "Fri, 3 Apr 2009 6:26:03 -0500",              1238757963 },

     /* A comment followed by the timezone  */
     { "Wed, 13 Jun 2007 12:34:56 (CET) +0100",      1181734496 },

     /* A timezone we do not understand, assume UTC */
     { "Wed, 13 Jun 2007 12:34:56 D",                1181738096 },

     /* Missing TZ, assume UTC */
     { "Wed, 13 Jun 2007 12:34:56 (CET)",            1181738096 },
     { "Fri, 10 Oct 2008 12:36:15",                  1223642175 },
     { "Sat, 16 Apr 2011 18:17:54 (GMT)",            1302977874 },
     { "Sat, 19 Mar 2011 22:46:08 (GMT)",            1300574768 },
     { "Sun, 24 Apr 2011 01:14:15 (GMT)",            1303607655 },
     { "Thu, 05 Jan 2012 02:11:51 (GMT)",            1325729511 },
     { "Thu, 21 Apr 2011 18:54:56 (GMT)",            1303412096 },
     { "Tue, 10 Jan 2012 14:31:05 (GMT)",            1326205865 },

     /* Stuff we don't parse */
     { "Wed, 13 Jun 2007 12.34 +0100",               -1         },
     { "Wed, 13 Jun 2007 -1:34:56 +0100",            -1         },
     { "Wed, 13 Jun 2007 24:34:56 +0100",            -1         },
     { "Wed, 13 (06) Jun 2007 24:34:56 +0100",       -1         },
     { "Wed, 13 Jun 2007 12:-1:56 +0100",            -1         },
     { "Wed, 13 Jun 2007 12:60:56 +0100",            -1         },
     { "Wed, 13 Jun 2007 (bar baz) 12:60:56 +0100",  -1         },
     { "Wed, 13 Jun 2007 12:34:-1 +0100",            -1         },
     { "Wed, 13 Jun 2007 12:34:61 +0100",            -1         },
     { "Wed, ab Jun 2007 12:34:56 +0100",            -1         },
     { "Wed, -2 Jun 2007 12:34:56 +0100",            -1         },
     { "Wed, 32 Jun 2007 12:34:56 +0100",            -1         },
     { "Wed, 13 Bob 2007 12:34:56 +0100",            -1         },
     { "Wed, 13 Jun -1 12:34:56 +0100",              -1         },
     { "Wed, 13 Jun 10000 12:34:56 +0100",           -1         },
     { "13 Jun 2007",                                -1         },
     { "2007 12 02",                                 -1         },
     { "Mon Nov 17 23:47:44 CST 2008",               -1         },
     { "Mon Nov 17 23:48:26 CST 2008",               -1         },
     { "Mon Nov 17 23:48:27 CST 2008",               -1         },
     { "Mon, 12 Mar 2012 17.58.37 +0100",            -1         },
     { "Mon, 2 Sep 2019 09:13:05 0000",              -1         },
     { "Mon, Jan 11 09:56:23 2016 +0100",            -1         },
     { "Mon,02 apr 2012 20:36:16 +0800",             -1         },
     { "Sun, Jan 10 07:54:56 CET 2016",              -1         },
     { "Thu Oct 16 22:10:38 EST 2008",               -1         },
     { "Thu Oct 23 11:55:16 EST 2008",               -1         },
     { "Thu, 10 Jun 2010 20:36:17 2000",             -1         },
     { "Thu, 22 Mar 2012 00:21:36 +00",              -1         },
     { "Tue, 17 Sep 2019 14:29:06 0000",             -1         },
     { "Vie, 16 Mar 2007 11:52:31 +0100",            -1         },
     { "Sun=2C 9 Jun 2013 20:18:43 +0100",           -1         },
  };
  // clang-format on

  {
    struct Tz tz;
    for (size_t i = 0; i < mutt_array_size(parse_tests); i++)
    {
      memset(&tz, 0, sizeof(tz));
      TEST_CASE(parse_tests[i].str);
      time_t result = mutt_date_parse_date(parse_tests[i].str, &tz);
      if (!TEST_CHECK(result == parse_tests[i].expected))
      {
        TEST_MSG("Expected: %ld", parse_tests[i].expected);
        TEST_MSG("Actual  : %ld", result);
      }
    }
  }
}
