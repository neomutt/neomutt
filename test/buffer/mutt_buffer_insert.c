/**
 * @file
 * Test code for mutt_buffer_insert()
 *
 * @authors
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
#include <stddef.h>
#include "mutt/lib.h"
#include "test_common.h"

struct InsertTest
{
  const char *orig;
  int position;
  const char *insert;
  const char *result;
};

void test_mutt_buffer_insert(void)
{
  // Degenerate tests
  {
    TEST_CHECK(mutt_buffer_insert(NULL, 0, NULL) == -1);
  }

  {
    struct Buffer buf = { 0 };
    TEST_CHECK(mutt_buffer_insert(&buf, 0, NULL) == -1);
  }

  {
    TEST_CHECK(mutt_buffer_insert(NULL, 0, "something") == -1);
  }

  static const struct InsertTest tests[] = {
    // clang-format off
    { NULL,          0,  "I",      "I"       },
    { NULL,          0,  "INSERT", "INSERT"  },
    { NULL,          1,  "I",      " I"      },
    { NULL,          1,  "INSERT", " INSERT" },

    { "a",           0,  "I",      "Ia"       },
    { "a",           0,  "INSERT", "INSERTa"  },
    { "a",           1,  "I",      "aI"       },
    { "a",           1,  "INSERT", "aINSERT"  },
    { "a",           2,  "I",      "a I"      },
    { "a",           2,  "INSERT", "a INSERT" },

    { "ab",          0,  "I",      "Iab"       },
    { "ab",          0,  "INSERT", "INSERTab"  },
    { "ab",          1,  "I",      "aIb"       },
    { "ab",          1,  "INSERT", "aINSERTb"  },
    { "ab",          2,  "I",      "abI"       },
    { "ab",          2,  "INSERT", "abINSERT"  },
    { "ab",          3,  "I",      "ab I"      },
    { "ab",          3,  "INSERT", "ab INSERT" },

    { "applebanana", 0,  "I",      "Iapplebanana"       },
    { "applebanana", 0,  "INSERT", "INSERTapplebanana"  },
    { "applebanana", 1,  "I",      "aIpplebanana"       },
    { "applebanana", 1,  "INSERT", "aINSERTpplebanana"  },
    { "applebanana", 5,  "I",      "appleIbanana"       },
    { "applebanana", 5,  "INSERT", "appleINSERTbanana"  },
    { "applebanana", 10, "I",      "applebananIa"       },
    { "applebanana", 10, "INSERT", "applebananINSERTa"  },
    { "applebanana", 11, "I",      "applebananaI"       },
    { "applebanana", 11, "INSERT", "applebananaINSERT"  },
    { "applebanana", 12, "I",      "applebanana I"      },
    { "applebanana", 12, "INSERT", "applebanana INSERT" },

    { NULL, 0, NULL, NULL },
    // clang-format on
  };

  {
    for (size_t i = 0; tests[i].result; i++)
    {
      TEST_CASE_("%d", i);
      struct Buffer *buf = mutt_buffer_pool_get();
      mutt_buffer_addstr(buf, tests[i].orig);
      mutt_buffer_insert(buf, tests[i].position, tests[i].insert);
      TEST_CHECK_STR_EQ(tests[i].result, mutt_buffer_string(buf));
      mutt_buffer_pool_release(&buf);
    }
  }

  {
    // Insertion that triggers a realloc
    struct Buffer *buf = mutt_buffer_pool_get();
    const size_t len = buf->dsize;
    for (size_t i = 0; i < len - 2; ++i)
    {
      mutt_buffer_addch(buf, 'A');
    }
    TEST_CHECK(buf->dsize == len);
    mutt_buffer_insert(buf, len / 2, "CDEFG");
    TEST_CHECK(buf->dsize != len);
    mutt_buffer_pool_release(&buf);
  }
}
