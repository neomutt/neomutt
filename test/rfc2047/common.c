/**
 * @file
 * Common code for RFC2047 tests
 *
 * @authors
 * Copyright (C) 2018 Pietro Cerutti <gahr@gahr.ch>
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
#include <locale.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "common.h"

// clang-format off
const struct Rfc2047TestData rfc2047_test_data[] =
{
  {
    /* The string is split in the middle of a multi-byte sequence */
      "=?UTF-8?Q?Kvie=C4=8Diame=20drauge=20pildyti=20ESO=20pasi=C5=BEad=C4?="
      "\n =?UTF-8?Q?=97jim=C5=B3=20girliand=C4=85!?="
    , "Kviečiame drauge pildyti ESO pasižadėjimų girliandą!"
    , "=?utf-8?Q?Kvie=C4=8Diame_drauge_pildyti_ESO_pasi=C5=BEad=C4=97jim=C5=B3_g?="
      "\n\t=?utf-8?Q?irliand=C4=85!?="
  },
  {
    /* Reduced test case for split multi-byte sequence */
      "=?utf-8?Q?=C4?==?utf-8?Q?=97?="
    , "ė"
    , "=?utf-8?B?xJc=?="
  },
  {
    /* Make sure spaces after an encoded word are kept */
      "=?utf-8?B?6IGq5piO55qE?=    Hello"
    , "聪明的    Hello"
    , "=?utf-8?B?6IGq5piO55qE?=    Hello"
  },
  {
    /* Make sure spaces before an encoded word are kept */
      "=?UTF-8?Q?Hello____=E8=81=AA=E6=98=8E=E7=9A=84?=" /* Roundcube style */
    , "Hello    聪明的"
    , "Hello    =?utf-8?B?6IGq5piO55qE?="
  },
  {
    /* Make sure spaces between encoded words are kept */
      "=?utf-8?B?6IGq5piO55qEICAgIOiBquaYjueahA==?="
    , "聪明的    聪明的"
    , "=?utf-8?B?6IGq5piO55qEICAgIOiBquaYjueahA==?="
  },
  {
    /* Let's accept spaces within encoded-text (issue #1189). In this
     * particular case, NeoMutt choses to encode only the initial part of the
     * string, as the remaining part only contains ASCII characters. */
      "=?UTF-8?Q?Sicherheitsl=C3=BCcke in praktisch allen IT-Systemen?="
    , "Sicherheitslücke in praktisch allen IT-Systemen"
    , "=?utf-8?Q?Sicherheitsl=C3=BCcke?= in praktisch allen IT-Systemen"
  },
  { NULL, NULL, NULL },
};
// clang-format on
