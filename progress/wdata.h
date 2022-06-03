/**
 * @file
 * Progress Bar Window Data
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_PROGRESS_WDATA_H
#define MUTT_PROGRESS_WDATA_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

struct MuttWindow;

/**
 * struct ProgressWindowData - Progress Bar Window Data
 */
struct ProgressWindowData
{
  struct MuttWindow *win;       ///< Window to draw on

  // Settings for the Progress Bar
  char   msg[1024];             ///< Message to display
  char   pretty_size[24];       ///< Pretty string for size
  size_t size;                  ///< Total expected size
  size_t size_inc;              ///< Size increment
  size_t time_inc;              ///< Time increment
  bool   is_bytes;              ///< true if measuring bytes

  // Current display
  size_t   display_pos;         ///< Displayed position
  int      display_percent;     ///< Displayed percentage complete
  uint64_t display_time;        ///< Time of last display
  char     pretty_pos[24];      ///< Pretty string for the position

  // Updates waiting for display
  size_t   update_pos;          ///< Updated position
  int      update_percent;      ///< Updated percentage complete
  uint64_t update_time;         ///< Time of last update
};

void                       progress_wdata_free(struct MuttWindow *win, void **ptr);
struct ProgressWindowData *progress_wdata_new (void);

#endif /* MUTT_PROGRESS_WDATA_H */
