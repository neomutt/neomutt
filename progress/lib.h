/**
 * @file
 * Progress bar
 *
 * @authors
 * Copyright (C) 2018-2021 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page lib_progress Progress
 *
 * Progress Bar
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
  MUTT_PROGRESS_READ,  ///< Progress tracks elements, according to `$read_inc`
  MUTT_PROGRESS_WRITE, ///< Progress tracks elements, according to `$write_inc`
  MUTT_PROGRESS_NET    ///< Progress tracks bytes, according to `$net_inc`
};

void             progress_free  (struct Progress **ptr);
struct Progress *progress_new   (const char *msg, enum ProgressType type, size_t size);
bool             progress_update(struct Progress *progress, size_t pos, int percent);

#endif /* MUTT_PROGRESS_LIB_H */
