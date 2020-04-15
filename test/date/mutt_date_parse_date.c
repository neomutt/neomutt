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

     /* This should be India Standard Time, but we use IST for Israel */
     { "Thu, 16 Aug 2001 16:01:08 IST+530",          997970468  },

     /* We only parse the MET part, we don't do -1 */
     { "Tue, 19 Dec 2000 19:33:09 MET-1",            977250789  },

     /* Skip unknown week days */
     { "Sunday, 21 Apr 2002 21:51:04 +0000",         1019425864 },
     { "Tuesday, 12 Feb 2002 13:08:10 -0500",        1013537290 },

     /* A partial TZ, assume UTC */
     { "Sun, 21 Apr 2002 21:51:04 +000",             1019425864 },

     /* A comment followed by the TZ */
     { "Wed, 13 Jun 2007 12:34:56 (CET) +0100",      1181734496 },

     /* No TZ, just a comment, assume UTC */
     { "Wed, 13 Jun 2007 12:34:56 (CET)",            1181738096 },

     /* No TZ, assume UTC */
     { "13 Jun 2007 12:34:56",                       1181738096 },

     /* A TZ we do not understand, assume UTC */
     { "Wed, 13 Jun 2007 12:34:56 D",                1181738096 },
     { "Tue, 19 Aug 2003 12:47:46 --0400",           1061297266 },
     { "Tue, 30 Mar 2004 14:33:08 %z (PST)",         1080657188 },

     /* Bad seconds, we skip the part we don't get  */
     { "Thu, 26 Mar 2020 17:16:22.020 +0000 (UTC)",  1585242982 },
     { "Wed, 13 Jun 2007 12:34:-1 +0100",            1181738040 },

     /* Single-digit time components */
     { "Fri, 6 Sep 2002 11:46:3 +0800",              1031283963 },
     { "Tue, 27 Apr 2004 16:2:37 +0800",             1083052957 },
     { "Fri, 5 Sep 2003 13:2:1 -0800",               1062795721 },
     { "Wed, 2 Oct 2002 2:7:1 +0800",                1033495621 },

     /* Missing space after the comma */
     { "Wed,17 Jul 2002 15:41:00 +0800",             1026891660 },

     /* Bad month case */
     { "Fri, 30 sep 2005 02:39:28 +0200",            -1         },

     /* Good stuff */
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

     /* Stuff we don't parse */
     { "Wed, ab Jun 2007 12:34:56 +0100",            -1         },
     { "Wed, -2 Jun 2007 12:34:56 +0100",            -1         },
     { "Wed, 32 Jun 2007 12:34:56 +0100",            -1         },
     { "Wed, 13 Bob 2007 12:34:56 +0100",            -1         },
     { "Wed, 13 Jun -1 12:34:56 +0100",              -1         },
     { "Wed, 13 Jun 10000 12:34:56 +0100",           -1         },
     { "Wed, 13 Jun 2007 12.34 +0100",               -1         },
     { "Wed, 13 Jun 2007 -1:34:56 +0100",            -1         },
     { "Wed, 13 Jun 2007 24:34:56 +0100",            -1         },
     { "Wed, 13 (06) Jun 2007 24:34:56 +0100",       -1         },
     { "Wed, 13 Jun 2007 12:-1:56 +0100",            -1         },
     { "Wed, 13 Jun 2007 12:60:56 +0100",            -1         },
     { "Wed, 13 Jun 2007 (bar baz) 12:60:56 +0100",  -1         },
     { "Wed, 13 Jun 2007 12:34:61 +0100",            -1         },
     { "13 Jun 2007",                                -1         },
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
