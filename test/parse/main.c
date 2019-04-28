/**
 * @file
 * Test code for parse Operations
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
  NEOMUTT_TEST_ITEM(test_mutt_auto_subscribe)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_check_encoding)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_check_mime_type)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_extract_message_id)                              \
  NEOMUTT_TEST_ITEM(test_mutt_is_message_type)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_matches_ignore)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_parse_content_type)                              \
  NEOMUTT_TEST_ITEM(test_mutt_parse_mailto)                                    \
  NEOMUTT_TEST_ITEM(test_mutt_parse_multipart)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_parse_part)                                      \
  NEOMUTT_TEST_ITEM(test_mutt_read_mime_header)                                \
  NEOMUTT_TEST_ITEM(test_mutt_rfc822_parse_line)                               \
  NEOMUTT_TEST_ITEM(test_mutt_rfc822_parse_message)                            \
  NEOMUTT_TEST_ITEM(test_mutt_rfc822_read_header)                              \
  NEOMUTT_TEST_ITEM(test_mutt_rfc822_read_line)

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

