/**
 * @file
 * Test code for mutt_mb_mbstowcs()
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

void test_mutt_mb_mbstowcs(void)
{
  // size_t mutt_mb_mbstowcs(wchar_t **pwbuf, size_t *pwbuflen, size_t i, char *buf);

  {
    size_t pwbuflen = 0;
    TEST_CHECK(mutt_mb_mbstowcs(NULL, &pwbuflen, 0, "apple") == 0);
  }

  {
    wchar_t *wbuf = NULL;
    TEST_CHECK(mutt_mb_mbstowcs(&wbuf, NULL, 0, "apple") == 0);
  }

  {
    wchar_t *wbuf = NULL;
    size_t pwbuflen = 0;
    TEST_CHECK(mutt_mb_mbstowcs(&wbuf, &pwbuflen, 0, NULL) == 0);
  }

  {
    struct WideTest
    {
      char *name;
      char *src;
      wchar_t *expected;
    };

    struct WideTest test[] = {
      // clang-format off
      { "Greek",     "Οὐχὶ ταὐτὰ παρίσταταί μοι γιγνώσκειν, ὦ ἄνδρες ᾿Αθηναῖοι",         L"Οὐχὶ ταὐτὰ παρίσταταί μοι γιγνώσκειν, ὦ ἄνδρες ᾿Αθηναῖοι" },
      { "Georgian",  "გთხოვთ ახლავე გაიაროთ რეგისტრაცია Unicode-ის მეათე საერთაშორისო",  L"გთხოვთ ახლავე გაიაროთ რეგისტრაცია Unicode-ის მეათე საერთაშორისო" },
      { "Russian",   "Зарегистрируйтесь сейчас на Десятую Международную Конференцию по", L"Зарегистрируйтесь сейчас на Десятую Международную Конференцию по" },
      { "Thai",      "๏ แผ่นดินฮั่นเสื่อมโทรมแสนสังเวช พระปกเกศกองบู๊กู้ขึ้นใหม่",                     L"๏ แผ่นดินฮั่นเสื่อมโทรมแสนสังเวช พระปกเกศกองบู๊กู้ขึ้นใหม่" },
      { "Ethiopian", "ሰማይ አይታረስ ንጉሥ አይከሰስ።",                                             L"ሰማይ አይታረስ ንጉሥ አይከሰስ።" },
      { "Braille",   "⡍⠜⠇⠑⠹ ⠺⠁⠎ ⠙⠑⠁⠙⠒ ⠞⠕ ⠃⠑⠛⠔ ⠺⠊⠹⠲ ⡹⠻⠑ ⠊⠎ ⠝⠕ ⠙⠳⠃⠞",                      L"⡍⠜⠇⠑⠹ ⠺⠁⠎ ⠙⠑⠁⠙⠒ ⠞⠕ ⠃⠑⠛⠔ ⠺⠊⠹⠲ ⡹⠻⠑ ⠊⠎ ⠝⠕ ⠙⠳⠃⠞" },
      { NULL },
      // clang-format on
    };

    for (size_t i = 0; test[i].src; i++)
    {
      TEST_CASE(test[i].name);
      wchar_t *result = NULL;
      size_t rlen = 0;

      size_t elen = wcslen(test[i].expected);
      TEST_CHECK(mutt_mb_mbstowcs(&result, &rlen, 0, test[i].src) == elen);
      TEST_CHECK(memcmp(result, test[i].expected, elen) == 0);
      FREE(&result);
    }
  }
}
