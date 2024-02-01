/**
 * @file
 * Test code for buf_inline_replace()
 *
 * @authors
 * Copyright (C) 2024 Dennis Schön <mail@dennis-schoen.de>
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

struct InlineReplaceTest
{
  const char *initial;
  size_t pos;
  size_t len;
  const char *replace;
  const char *expected;
};

void test_buf_inline_replace(void)
{
  // void buf_inline_replace(struct Buffer *buf, size_t pos, size_t len, const char *str);

  // clang-format off
  static const struct InlineReplaceTest replace_tests[] =
  {
    { NULL,         0, 2, "apple",     ""                },
    { "apple",      0, 2, NULL,        "apple"           },

    { "XXXXbanana", 0, 4, "",          "banana"          },
    { "XXXXbanana", 0, 4, "OO",        "OObanana"        },
    { "XXXXbanana", 0, 4, "OOOO",      "OOOObanana"      },
    { "XXXXbanana", 0, 4, "OOOOOO",    "OOOOOObanana"    },
    { "XXXXbanana", 0, 4, "OOOOOOO",   "OOOOOOObanana"   },
    { "XXXXbanana", 0, 4, "OOOOOOOO",  "OOOOOOOObanana"  },

    { "banXXXXana", 3, 4, "",          "banana"          },
    { "banXXXXana", 3, 4, "OO",        "banOOana"        },
    { "banXXXXana", 3, 4, "OOOO",      "banOOOOana"      },
    { "banXXXXana", 3, 4, "OOOOOO",    "banOOOOOOana"    },
    { "banXXXXana", 3, 4, "OOOOOOO",   "banOOOOOOOana"   },
    { "banXXXXana", 3, 4, "OOOOOOOO",  "banOOOOOOOOana"  },

    { "bananaXXXX", 6, 4, "",          "banana"          },
    { "bananaXXXX", 6, 4, "OO",        "bananaOO"        },
    { "bananaXXXX", 6, 4, "OOOO",      "bananaOOOO"      },
    { "bananaXXXX", 6, 4, "OOOOOO",    "bananaOOOOOO"    },
    { "bananaXXXX", 6, 4, "OOOOOOO",   "bananaOOOOOOO"   },
    { "bananaXXXX", 6, 4, "OOOOOOOO",  "bananaOOOOOOOO"  },

    { "banana",     0, 0, "",          "banana"          },
    { "banana",     0, 0, "OO",        "OObanana"        },
    { "banana",     0, 0, "OOOO",      "OOOObanana"      },
    { "banana",     0, 0, "OOOOOO",    "OOOOOObanana"    },
    { "banana",     0, 0, "OOOOOOO",   "OOOOOOObanana"   },
    { "banana",     0, 0, "OOOOOOOO",  "OOOOOOOObanana"  },

    { "banana",     3, 0, "",          "banana"          },
    { "banana",     3, 0, "OO",        "banOOana"        },
    { "banana",     3, 0, "OOOO",      "banOOOOana"      },
    { "banana",     3, 0, "OOOOOO",    "banOOOOOOana"    },
    { "banana",     3, 0, "OOOOOOO",   "banOOOOOOOana"   },
    { "banana",     3, 0, "OOOOOOOO",  "banOOOOOOOOana"  },

    { "banana",     6, 0, "",          "banana"          },
    { "banana",     6, 0, "OO",        "bananaOO"        },
    { "banana",     6, 0, "OOOO",      "bananaOOOO"      },
    { "banana",     6, 0, "OOOOOO",    "bananaOOOOOO"    },
    { "banana",     6, 0, "OOOOOOO",   "bananaOOOOOOO"   },
    { "banana",     6, 0, "OOOOOOOO",  "bananaOOOOOOOO"  },
  };
  // clang-format on

  {
    for (size_t i = 0; i < mutt_array_size(replace_tests); i++)
    {
      const struct InlineReplaceTest *t = &replace_tests[i];

      struct Buffer *buf = NULL;
      if (t->initial)
        buf = buf_new(t->initial);

      TEST_CASE_("'%s', %d, %d, '%s'", t->initial, t->pos, t->len, t->replace);

      buf_inline_replace(buf, t->pos, t->len, t->replace);
      TEST_CHECK_STR_EQ(buf_string(buf), t->expected);

      buf_free(&buf);
    }
  }
}
