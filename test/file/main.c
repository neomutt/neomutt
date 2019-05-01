/**
 * @file
 * Test code for file Operations
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
  NEOMUTT_TEST_ITEM(test_mutt_file_check_empty)                                \
  NEOMUTT_TEST_ITEM(test_mutt_file_chmod_add)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_file_chmod_add_stat)                             \
  NEOMUTT_TEST_ITEM(test_mutt_file_chmod)                                      \
  NEOMUTT_TEST_ITEM(test_mutt_file_chmod_rm)                                   \
  NEOMUTT_TEST_ITEM(test_mutt_file_chmod_rm_stat)                              \
  NEOMUTT_TEST_ITEM(test_mutt_file_copy_bytes)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_file_copy_stream)                                \
  NEOMUTT_TEST_ITEM(test_mutt_file_decrease_mtime)                             \
  NEOMUTT_TEST_ITEM(test_mutt_file_expand_fmt)                                 \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_file_expand_fmt_quote)                    \
  NEOMUTT_TEST_ITEM(test_mutt_file_fclose)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_file_fopen)                                      \
  NEOMUTT_TEST_ITEM(test_mutt_file_fsync_close)                                \
  NEOMUTT_TEST_ITEM(test_mutt_file_get_size)                                   \
  NEOMUTT_TEST_ITEM(test_mutt_file_get_stat_timespec)                          \
  NEOMUTT_TEST_ITEM(test_mutt_file_iter_line)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_file_lock)                                       \
  NEOMUTT_TEST_ITEM(test_mutt_file_map_lines)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_file_mkdir)                                      \
  NEOMUTT_TEST_ITEM(test_mutt_file_mkstemp_full)                               \
  NEOMUTT_TEST_ITEM(test_mutt_file_open)                                       \
  NEOMUTT_TEST_ITEM(test_mutt_file_quote_filename)                             \
  NEOMUTT_TEST_ITEM(test_mutt_buffer_quote_filename)                           \
  NEOMUTT_TEST_ITEM(test_mutt_file_read_keyword)                               \
  NEOMUTT_TEST_ITEM(test_mutt_file_read_line)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_file_rename)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_file_rmtree)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_file_safe_rename)                                \
  NEOMUTT_TEST_ITEM(test_mutt_file_sanitize_filename)                          \
  NEOMUTT_TEST_ITEM(test_mutt_file_sanitize_regex)                             \
  NEOMUTT_TEST_ITEM(test_mutt_file_set_mtime)                                  \
  NEOMUTT_TEST_ITEM(test_mutt_file_stat_compare)                               \
  NEOMUTT_TEST_ITEM(test_mutt_file_stat_timespec_compare)                      \
  NEOMUTT_TEST_ITEM(test_mutt_file_symlink)                                    \
  NEOMUTT_TEST_ITEM(test_mutt_file_timespec_compare)                           \
  NEOMUTT_TEST_ITEM(test_mutt_file_touch_atime)                                \
  NEOMUTT_TEST_ITEM(test_mutt_file_unlink)                                     \
  NEOMUTT_TEST_ITEM(test_mutt_file_unlink_empty)                               \
  NEOMUTT_TEST_ITEM(test_mutt_file_unlock)

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
