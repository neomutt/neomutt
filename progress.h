/**
 * @file
 * Progress bar
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
#include "mutt/mutt.h"

/* These Config Variables are only used in progress.c */
extern short C_TimeInc;

#define MUTT_PROGRESS_SIZE (1 << 0) /**< traffic-based progress */
#define MUTT_PROGRESS_MSG  (1 << 1) /**< message-based progress */

/**
 * struct Progress - A progress bar
 */
struct Progress
{
  unsigned short inc;
  unsigned short flags;
  const char *msg;
  long pos;
  size_t size;
  unsigned int timestamp;
  char sizestr[SHORT_STRING];
};

void mutt_progress_init(struct Progress *progress, const char *msg,
                        unsigned short flags, unsigned short inc, size_t size);
void mutt_progress_update(struct Progress *progress, long pos, int percent);

#endif /* MUTT_PROGRESS_H */
