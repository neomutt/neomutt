/**
 * @file
 * Test code for buffer Operations
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
  NEOMUTT_TEST_ITEM(test_mutt_buffer_addch)                                    \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_add_printf)                               \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_addstr)                                   \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_addstr_n)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_alloc)                                    \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_concat_path)                              \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_fix_dptr)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_free)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_from)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_increase_size)                            \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_init)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_is_empty)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_len)                                      \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_new)                                      \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_pool_free)                                \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_pool_get)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_pool_release)                             \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_printf)                                   \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_reset)                                    \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_strcpy)                                   \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_strcpy_n)

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
