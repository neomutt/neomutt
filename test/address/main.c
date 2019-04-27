/**
 * @file
 * Test code for Path Operations
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
  NEOMUTT_TEST_ITEM(test_mutt_addr_append)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_addr_cat)                                        \
  NEOMUTT_TEST_ITEM(test_mutt_addr_cmp)                                        \
  NEOMUTT_TEST_ITEM(test_mutt_addr_cmp_strict)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_addr_copy)                                       \
  NEOMUTT_TEST_ITEM(test_mutt_addr_copy_list)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_addr_for_display)                                \
  NEOMUTT_TEST_ITEM(test_mutt_addr_free)                                       \
  NEOMUTT_TEST_ITEM(test_mutt_addr_has_recips)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_addr_is_intl)                                    \
  NEOMUTT_TEST_ITEM(test_mutt_addr_is_local)                                   \
  NEOMUTT_TEST_ITEM(test_mutt_addr_mbox_to_udomain)                            \
  NEOMUTT_TEST_ITEM(test_mutt_addr_new)                                        \
  NEOMUTT_TEST_ITEM(test_mutt_addr_parse_list)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_addr_parse_list2)                                \
  NEOMUTT_TEST_ITEM(test_mutt_addr_qualify)                                    \
  NEOMUTT_TEST_ITEM(test_mutt_addr_remove_from_list)                           \
  NEOMUTT_TEST_ITEM(test_mutt_addr_remove_xrefs)                               \
  NEOMUTT_TEST_ITEM(test_mutt_addr_search)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_addr_set_intl)                                   \
  NEOMUTT_TEST_ITEM(test_mutt_addr_set_local)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_addr_valid_msgid)                                \
  NEOMUTT_TEST_ITEM(test_mutt_addr_write)                                      \
  NEOMUTT_TEST_ITEM(test_mutt_addr_write_single)                               \
  NEOMUTT_TEST_ITEM(test_mutt_addrlist_to_intl)                                \
  NEOMUTT_TEST_ITEM(test_mutt_addrlist_to_local)

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
