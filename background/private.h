/**
 * @file
 * Background commands - private data
 *
 * @authors
 * Copyright (C) 2026 Reza Jelveh
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

#ifndef MUTT_BACKGROUND_PRIVATE_H
#define MUTT_BACKGROUND_PRIVATE_H

#include <stdbool.h>
#include "mutt/lib.h"

/// Maximum number of jobs kept at once; Clear frees the finished ones
#define MAX_JOBS 10

/**
 * struct BackgroundJob - A command running in the background
 */
struct BackgroundJob
{
  pid_t pid;           ///< Process id
  bool running;        ///< true if the process is still running
  int exit_code;       ///< Exit code, -1 if still running or killed by a signal
  struct Buffer *cmd;  ///< Command line
  struct Buffer *file; ///< Temp file with captured output while running

  struct Buffer *out;  ///< Output kept in memory after completion
};

extern struct BackgroundJob Jobs[MAX_JOBS];

// background.c
void bg_save_output(const struct BackgroundJob *job);
void bg_view_output(const struct BackgroundJob *job);
void job_slot_free(struct BackgroundJob *job);

// dlg_background.c
int *bg_rows_build(int *n_rows);

#endif /* MUTT_BACKGROUND_PRIVATE_H */
