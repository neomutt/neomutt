/**
 * @file
 * Test code for mutt_buffer_concat_path()
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
#include "acutest.h"
#include "config.h"
#include "mutt/mutt.h"

void test_mutt_buffer_concat_path(void)
{
  // void mutt_buffer_concat_path(struct Buffer *buf, const char *dir, const char *fname);

  {
    mutt_buffer_concat_path(NULL, "apple", "banana");
    TEST_CHECK_(1, "mutt_buffer_concat_path(NULL, \"apple\", \"banana\")");
  }

  // clang-format off
  const char *dirs[] = { NULL, "", "dir", "dir/" };
  const char *files[] = { NULL, "", "file" };
  const char *concat_test[][3] = {
    { dirs[0], files[0], NULL       },
    { dirs[0], files[1], NULL       },
    { dirs[0], files[2], "file"     },
    { dirs[1], files[0], NULL       },
    { dirs[1], files[1], NULL       },
    { dirs[1], files[2], "file"     },
    { dirs[2], files[0], "dir"      },
    { dirs[2], files[1], "dir"      },
    { dirs[2], files[2], "dir/file" },
    { dirs[3], files[0], "dir/"     },
    { dirs[3], files[1], "dir/"     },
    { dirs[3], files[2], "dir/file" },
  };
  // clang-format on

  {
    for (size_t i = 0; i < mutt_array_size(concat_test); i++)
    {
      TEST_CASE_("DIR: '%s'  FILE: '%s'", NONULL(concat_test[i][0]), NONULL(concat_test[i][1]));
      {
        struct Buffer *buf = mutt_buffer_new();
        mutt_buffer_concat_path(buf, concat_test[i][0], concat_test[i][1]);
        if (concat_test[i][2])
        {
          TEST_CHECK(strcmp(mutt_b2s(buf), concat_test[i][2]) == 0);
        }
        else
        {
          if (!TEST_CHECK(strlen(mutt_b2s(buf)) == 0))
          {
            TEST_MSG("len = %ld", strlen(mutt_b2s(buf)));
          }
        }
        mutt_buffer_free(&buf);
      }

      {
        const char *str = "test";
        struct Buffer *buf = mutt_buffer_from(str);
        mutt_buffer_concat_path(buf, concat_test[i][0], concat_test[i][1]);
        if (concat_test[i][2])
        {
          TEST_CHECK(strcmp(mutt_b2s(buf), concat_test[i][2]) == 0);
        }
        else
        {
          TEST_CHECK(strcmp(mutt_b2s(buf), str) == 0);
        }
        mutt_buffer_free(&buf);
      }
    }
  }
}

