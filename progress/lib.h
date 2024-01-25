/**
 * @file
 * Progress Bar
 *
 * @authors
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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
 * @page lib_progress Progress Bar
 *
 * Display a colourful Progress Bar with text.
 *
 * The Progress Bar can be used to display the progress of a Read, Write or Net
 * operation.  It may look like this:
 *
 * ```
 * Reading from cache 34/125 (27%)
 * ```
 *
 * The Progress Bar can be used in three ways, depending on what's expected.
 *
 * - Unknown amount of data
 *   - Call `progress_new(0)`
 *   - Call `progress_update(n,-1)` -- records/bytes so far
 *
 * - Fixed number of records/bytes
 *   - Call `progress_new(n)` -- number of records/bytes
 *   - Call `progress_update(n,-1)` -- records/bytes so far
 *
 * - Percentage
 *   - Call `progress_new(0)`
 *   - Call `progress_update(n,pc)` -- records/bytes so far, percentage
 *
 * The frequency of screen updates can be configured.
 * For each variable, a value of 0 means show **every** update.
 *
 * - `$net_inc`   -- Update after this many KB sent/received
 * - `$read_inc`  -- Update after this many records read
 * - `$write_inc` -- Update after this many records written
 *
 * Additionally,
 *
 * - `$time_inc` - Frequency of progress bar updates (milliseconds)
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | progress/progress.c | @subpage progress_progress |
 * | progress/wdata.c    | @subpage progress_wdata    |
 * | progress/window.c   | @subpage progress_window   |
 */

#ifndef MUTT_PROGRESS_LIB_H
#define MUTT_PROGRESS_LIB_H

#include <stdbool.h>
#include <stdio.h>

struct Progress;

/**
 * enum ProgressType - What kind of operation is this progress tracking?
 */
enum ProgressType
{
  MUTT_PROGRESS_NET,   ///< Progress tracks bytes,    according to `$net_inc`
  MUTT_PROGRESS_READ,  ///< Progress tracks elements, according to `$read_inc`
  MUTT_PROGRESS_WRITE, ///< Progress tracks elements, according to `$write_inc`
};

void             progress_free       (struct Progress **ptr);
struct Progress *progress_new        (enum ProgressType type, size_t size);
bool             progress_update     (struct Progress *progress, size_t pos, int percent);
void             progress_set_message(struct Progress *progress, const char *fmt, ...)
                                      __attribute__((__format__(__printf__, 2, 3)));
void             progress_set_size   (struct Progress *progress, size_t size);

#endif /* MUTT_PROGRESS_LIB_H */
