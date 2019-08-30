/**
 * @file
 * Progress bar
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_PROGRESS_H
#define MUTT_PROGRESS_H

#include <stdio.h>

/* These Config Variables are only used in progress.c */
extern short C_TimeInc;
extern short C_ReadInc;
extern short C_WriteInc;
extern short C_NetInc;

/**
 * Enum ProgressType - What kind of operation is this progress tracking?
 */
enum ProgressType
{
  MUTT_PROGRESS_READ,  ///< Progress tracks elements, according to C_ReadInc
  MUTT_PROGRESS_WRITE, ///< Progress tracks elements, according to C_WriteInc
  MUTT_PROGRESS_NET    ///< Progress tracks bytes, according to C_NetInc
};

/**
 * struct Progress - A progress bar
 */
struct Progress
{
  char msg[1024];
  char sizestr[24];
  size_t pos;
  size_t size;
  size_t inc;
  long timestamp;
  bool is_bytes;
};

void mutt_progress_init(struct Progress *progress, const char *msg, enum ProgressType type, size_t size);
void mutt_progress_update(struct Progress *progress, size_t pos, int percent);

#endif /* MUTT_PROGRESS_H */
