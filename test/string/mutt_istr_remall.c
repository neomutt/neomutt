/**
 * @file
 * Test code for mutt_istr_remall()
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

struct RemallTest
{
  const char *str;
  const char *expected;
};

void test_mutt_istr_remall(void)
{
  // int mutt_istr_remall(char *str, const char *target);

  {
    TEST_CHECK(mutt_istr_remall(NULL, "apple") == 1);
  }

  {
    TEST_CHECK(mutt_istr_remall("apple", NULL) == 1);
  }

  {
    TEST_CHECK(mutt_istr_remall(NULL, NULL) == 1);
  }

  // clang-format off
  struct RemallTest remall_tests[] =
  {
    { "",                        ""         },
    { "hello",                   "hello"    },
    { "apple",                   ""         },
    { "aPpLE",                   ""         },

    { "applebye",                "bye"      },
    { "AppLEBye",                "Bye"      },
    { "helloapplebye",           "hellobye" },
    { "hellOAPplEBye",           "hellOBye" },
    { "helloapple",              "hello"    },
    { "hellOAPpLE",              "hellO"    },

    { "appleApplebye",           "bye"      },
    { "AppLEAppLEBye",           "Bye"      },
    { "helloAppLEapplebye",      "hellobye" },
    { "hellOAPplEAppLEBye",      "hellOBye" },
    { "helloappleAppLE",         "hello"    },
    { "hellOAPpLEAPPLE",         "hellO"    },

    { "APpLEAPPLEApplEAPPle",    ""         },
  };
  // clang-format on

  {
    const char *remove = "apple";
    char buf[64];

    for (size_t i = 0; i < mutt_array_size(remall_tests); i++)
    {
      struct RemallTest *t = &remall_tests[i];
      memset(buf, 0, sizeof(buf));
      mutt_str_copy(buf, t->str, sizeof(buf));
      TEST_CASE(buf);

      mutt_istr_remall(buf, remove);
      TEST_CHECK(strcmp(buf, t->expected) == 0);
    }
  }
}
