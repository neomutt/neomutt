/**
 * @file
 * Test code for buf_mb_wcstombs()
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
#include <string.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "test_common.h"

void test_buf_mb_wcstombs(void)
{
  // void buf_mb_wcstombs(struct Buffer *dest, const wchar_t *wstr, size_t wlen);

  {
    wchar_t src[32] = L"apple";
    buf_mb_wcstombs(NULL, src, 5);
    TEST_CHECK_(1, "buf_mb_wcstombs(NULL, 10, src, 5)");
  }

  {
    struct Buffer *buf = buf_pool_get();
    buf_mb_wcstombs(buf, NULL, 3);
    TEST_CHECK_(1, "buf_mb_wcstombs(buf, sizeof(buf), NULL, 3)");
    buf_pool_release(&buf);
  }

  {
    struct WideTest
    {
      char *name;
      wchar_t *src;
      char *expected;
    };

    struct WideTest test[] = {
      // clang-format off
      { "Greek",     L"Οὐχὶ ταὐτὰ παρίσταταί μοι γιγνώσκειν, ὦ ἄνδρες ᾿Αθηναῖοι",         "Οὐχὶ ταὐτὰ παρίσταταί μοι γιγνώσκειν, ὦ ἄνδρες ᾿Αθηναῖοι" },
      { "Georgian",  L"გთხოვთ ახლავე გაიაროთ რეგისტრაცია Unicode-ის მეათე საერთაშორისო",  "გთხოვთ ახლავე გაიაროთ რეგისტრაცია Unicode-ის მეათე საერთაშორისო" },
      { "Russian",   L"Зарегистрируйтесь сейчас на Десятую Международную Конференцию по", "Зарегистрируйтесь сейчас на Десятую Международную Конференцию по" },
      { "Thai",      L"๏ แผ่นดินฮั่นเสื่อมโทรมแสนสังเวช พระปกเกศกองบู๊กู้ขึ้นใหม่",                     "๏ แผ่นดินฮั่นเสื่อมโทรมแสนสังเวช พระปกเกศกองบู๊กู้ขึ้นใหม่" },
      { "Ethiopian", L"ሰማይ አይታረስ ንጉሥ አይከሰስ።",                                             "ሰማይ አይታረስ ንጉሥ አይከሰስ።" },
      { "Braille",   L"⡍⠜⠇⠑⠹ ⠺⠁⠎ ⠙⠑⠁⠙⠒ ⠞⠕ ⠃⠑⠛⠔ ⠺⠊⠹⠲ ⡹⠻⠑ ⠊⠎ ⠝⠕ ⠙⠳⠃⠞",                      "⡍⠜⠇⠑⠹ ⠺⠁⠎ ⠙⠑⠁⠙⠒ ⠞⠕ ⠃⠑⠛⠔ ⠺⠊⠹⠲ ⡹⠻⠑ ⠊⠎ ⠝⠕ ⠙⠳⠃⠞" },
      { NULL },
      // clang-format on
    };

    struct Buffer *buf = buf_pool_get();
    for (size_t i = 0; test[i].src; i++)
    {
      buf_reset(buf);
      TEST_CASE(test[i].name);
      size_t len = wcslen(test[i].src);
      buf_mb_wcstombs(buf, test[i].src, len);

      TEST_CHECK_STR_EQ(buf_string(buf), test[i].expected);
    }
    buf_pool_release(&buf);
  }
}
