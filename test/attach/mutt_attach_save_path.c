/**
 * @file
 * Test code for mutt_attach_save_path()
 *
 * @authors
 * Copyright (C) 2026 Alejandro Pacheco <alepacheco99@gmail.com>
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
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "attach/lib.h"
#include "test_common.h"

void test_mutt_attach_save_path(void)
{
  // void mutt_attach_save_path(struct Buffer *buf, struct Body *b, const char *dir);

  static const char *tests[][3] = {
    // clang-format off
    { "apple.txt",                 NULL,      "/save/apple.txt"   },
    { "/tmp/apple.txt",            NULL,      "/save/apple.txt"   },
    { "../../apple.txt",           NULL,      "/save/apple.txt"   },
    { "banana/../../apple.txt",    NULL,      "/save/apple.txt"   },
    { "apple.txt",                 "/dir",    "/dir/apple.txt"    },
    { "/tmp/apple.txt",            "/dir",    "/dir/apple.txt"    },
    { "../../apple.txt",           "/dir",    "/dir/apple.txt"    },
    { "",                          NULL,      ""                  },
    { "/",                         NULL,      ""                  },
    { "apple/",                    NULL,      ""                  },
    { "../../apple/",              NULL,      ""                  },
    { ".",                         NULL,      ""                  },
    { "..",                        NULL,      ""                  },
    { "../..",                     NULL,      ""                  },
    { "..",                        "/dir",    ""                  },
    { "/tmp/apple/",               "/dir",    ""                  },
    // clang-format on
  };

  cs_subset_str_string_set(NeoMutt->sub, "attach_save_dir", "/save", NULL);

  {
    struct Buffer *buf = buf_pool_get();

    for (int i = 0; i < countof(tests); i++)
    {
      struct Body b = { 0 };
      b.filename = (char *) tests[i][0];
      const char *dir = tests[i][1];
      const char *expected = tests[i][2];
      TEST_CASE(b.filename);

      mutt_attach_save_path(buf, &b, dir);
      TEST_CHECK_STR_EQ(buf_string(buf), expected);
    }

    buf_pool_release(&buf);
  }

  {
    struct Buffer *buf = buf_pool_get();
    struct Body b = { 0 };

    mutt_attach_save_path(buf, &b, NULL);
    TEST_CHECK(buf_is_empty(buf));

    buf_pool_release(&buf);
  }

  cs_subset_str_string_set(NeoMutt->sub, "attach_save_dir", "./", NULL);
}
