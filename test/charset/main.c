/**
 * @file
 * Test code for charset Operations
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

#include "acutest.h"

/******************************************************************************
 * Add your test cases to this list.
 *****************************************************************************/
#define NEOMUTT_TEST_LIST                                                      \
  NEOMUTT_TEST_ITEM(test_mutt_ch_canonical_charset)                            \
  NEOMUTT_TEST_ITEM(test_mutt_ch_charset_lookup)                               \
  NEOMUTT_TEST_ITEM(test_mutt_ch_check)                                        \
  NEOMUTT_TEST_ITEM(test_mutt_ch_check_charset)                                \
  NEOMUTT_TEST_ITEM(test_mutt_ch_choose)                                       \
  NEOMUTT_TEST_ITEM(test_mutt_ch_chscmp)                                       \
  NEOMUTT_TEST_ITEM(test_mutt_ch_convert_nonmime_string)                       \
  NEOMUTT_TEST_ITEM(test_mutt_ch_convert_string)                               \
  NEOMUTT_TEST_ITEM(test_mutt_ch_fgetconv)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_ch_fgetconv_close)                               \
  NEOMUTT_TEST_ITEM(test_mutt_ch_fgetconv_open)                                \
  NEOMUTT_TEST_ITEM(test_mutt_ch_fgetconvs)                                    \
  NEOMUTT_TEST_ITEM(test_mutt_ch_get_default_charset)                          \
  NEOMUTT_TEST_ITEM(test_mutt_ch_get_langinfo_charset)                         \
  NEOMUTT_TEST_ITEM(test_mutt_ch_iconv)                                        \
  NEOMUTT_TEST_ITEM(test_mutt_ch_iconv_lookup)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_ch_iconv_open)                                   \
  NEOMUTT_TEST_ITEM(test_mutt_ch_lookup_add)                                   \
  NEOMUTT_TEST_ITEM(test_mutt_ch_lookup_remove)                                \
  NEOMUTT_TEST_ITEM(test_mutt_ch_set_charset)

/******************************************************************************
 * You probably don't need to touch what follows.
 *****************************************************************************/
#define NEOMUTT_TEST_ITEM(x) void x(void);
NEOMUTT_TEST_LIST
#undef NEOMUTT_TEST_ITEM

TEST_LIST = {
#define NEOMUTT_TEST_ITEM(x) { #x, x },
  NEOMUTT_TEST_LIST
#undef NEOMUTT_TEST_ITEM
  { 0 }
};
