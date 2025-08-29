/**
 * @file
 * Convenience wrapper for the library headers
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

/**
 * @page lib_mutt Mutt
 *
 * Shared code for handling strings, files, dates, etc
 *
 * Each source file in the library provides a group of related functions.
 *
 * | File             | Description             |
 * | :--------------- | :---------------------- |
 * | mutt/array.h     | @subpage mutt_array     |
 * | mutt/atoi.c      | @subpage mutt_atoi      |
 * | mutt/base64.c    | @subpage mutt_base64    |
 * | mutt/buffer.c    | @subpage mutt_buffer    |
 * | mutt/charset.c   | @subpage mutt_charset   |
 * | mutt/date.c      | @subpage mutt_date      |
 * | mutt/envlist.c   | @subpage mutt_envlist   |
 * | mutt/eqi.h       | @subpage mutt_eqi       |
 * | mutt/exit.c      | @subpage mutt_exit      |
 * | mutt/file.c      | @subpage mutt_file      |
 * | mutt/filter.c    | @subpage mutt_filter    |
 * | mutt/hash.c      | @subpage mutt_hash      |
 * | mutt/list.c      | @subpage mutt_list      |
 * | mutt/logging.c   | @subpage mutt_logging   |
 * | mutt/mapping.c   | @subpage mutt_mapping   |
 * | mutt/mbyte.c     | @subpage mutt_mbyte     |
 * | mutt/md5.c       | @subpage mutt_md5       |
 * | mutt/memory.c    | @subpage mutt_memory    |
 * | mutt/notify.c    | @subpage mutt_notify    |
 * | mutt/path.c      | @subpage mutt_path      |
 * | mutt/pool.c      | @subpage mutt_pool      |
 * | mutt/prex.c      | @subpage mutt_prex      |
 * | mutt/qsort_r.c   | @subpage mutt_qsort_r   |
 * | mutt/random.c    | @subpage mutt_random    |
 * | mutt/regex.c     | @subpage mutt_regex     |
 * | mutt/signal.c    | @subpage mutt_signal    |
 * | mutt/slist.c     | @subpage mutt_slist     |
 * | mutt/state.c     | @subpage mutt_state     |
 * | mutt/string.c    | @subpage mutt_string    |
 *
 * @note The library is self-contained -- some files may depend on others in
 *       the library, but none depends on source from outside.
 *
 * @image html libmutt.svg
 */

#ifndef MUTT_MUTT_LIB_H
#define MUTT_MUTT_LIB_H

#include "config.h"
// IWYU pragma: begin_keep
#include "array.h"
#include "atoi.h"
#include "base64.h"
#include "buffer.h"
#include "charset.h"
#include "date.h"
#include "envlist.h"
#include "eqi.h"
#include "exit.h"
#include "file.h"
#include "filter.h"
#include "hash.h"
#include "list.h"
#include "logging2.h"
#include "mapping.h"
#include "mbyte.h"
#include "md5.h"
#include "memory.h"
#include "message.h"
#include "notify.h"
#include "notify_type.h"
#include "observer.h"
#include "path.h"
#include "pool.h"
#include "prex.h"
#include "qsort_r.h"
#include "queue.h"
#include "random.h"
#include "regex3.h"
#include "signal2.h"
#include "slist.h"
#include "state.h"
#include "string2.h"
// IWYU pragma: end_keep

#if defined(COMPILER_IS_CLANG) || defined(COMPILER_IS_GCC)
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH
#endif

#endif /* MUTT_MUTT_LIB_H */
