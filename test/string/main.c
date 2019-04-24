/**
 * @file
 * Test code for String Operations
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
  NEOMUTT_TEST_ITEM(test_mutt_str_adjust)                                      \
  NEOMUTT_TEST_ITEM(test_mutt_str_append_item)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_str_asprintf)                                    \
  NEOMUTT_TEST_ITEM(test_mutt_str_atoi)                                        \
  NEOMUTT_TEST_ITEM(test_mutt_str_atol)                                        \
  NEOMUTT_TEST_ITEM(test_mutt_str_atos)                                        \
  NEOMUTT_TEST_ITEM(test_mutt_str_atoui)                                       \
  NEOMUTT_TEST_ITEM(test_mutt_str_atoul)                                       \
  NEOMUTT_TEST_ITEM(test_mutt_str_atoull)                                      \
  NEOMUTT_TEST_ITEM(test_mutt_str_dequote_comment)                             \
  NEOMUTT_TEST_ITEM(test_mutt_str_find_word)                                   \
  NEOMUTT_TEST_ITEM(test_mutt_str_getenv)                                      \
  NEOMUTT_TEST_ITEM(test_mutt_str_inline_replace)                              \
  NEOMUTT_TEST_ITEM(test_mutt_str_is_ascii)                                    \
  NEOMUTT_TEST_ITEM(test_mutt_str_is_email_wsp)                                \
  NEOMUTT_TEST_ITEM(test_mutt_str_lws_len)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_str_lws_rlen)                                    \
  NEOMUTT_TEST_ITEM(test_mutt_str_next_word)                                   \
  NEOMUTT_TEST_ITEM(test_mutt_str_pretty_size)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_str_remall_strcasestr)                           \
  NEOMUTT_TEST_ITEM(test_mutt_str_remove_trailing_ws)                          \
  NEOMUTT_TEST_ITEM(test_mutt_str_replace)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_str_rstrnstr)                                    \
  NEOMUTT_TEST_ITEM(test_mutt_str_skip_email_wsp)                              \
  NEOMUTT_TEST_ITEM(test_mutt_str_skip_whitespace)                             \
  NEOMUTT_TEST_ITEM(test_mutt_str_split)                                       \
  NEOMUTT_TEST_ITEM(test_mutt_str_startswith)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_str_strcasecmp)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_str_strcasestr)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_str_strcat)                                      \
  NEOMUTT_TEST_ITEM(test_mutt_str_strchrnul)                                   \
  NEOMUTT_TEST_ITEM(test_mutt_str_strcmp)                                      \
  NEOMUTT_TEST_ITEM(test_mutt_str_strcoll)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_str_strdup)                                      \
  NEOMUTT_TEST_ITEM(test_mutt_str_strfcpy)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_str_stristr)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_str_strlen)                                      \
  NEOMUTT_TEST_ITEM(test_mutt_str_strlower)                                    \
  NEOMUTT_TEST_ITEM(test_mutt_str_strncasecmp)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_str_strncat)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_str_strncmp)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_str_strnfcpy)                                    \
  NEOMUTT_TEST_ITEM(test_mutt_str_substr_cpy)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_str_substr_dup)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_str_sysexit)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_str_word_casecmp)

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
