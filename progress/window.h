/**
 * @file
 * Progress Bar Window
 *
 * @authors
 * Copyright (C) 2022-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_PROGRESS_WINDOW_H
#define MUTT_PROGRESS_WINDOW_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

struct MuttWindow;

struct MuttWindow *progress_window_new        (size_t size, size_t size_inc, size_t time_inc, bool is_bytes);
void               progress_window_set_message(struct MuttWindow *win, const char *fmt, va_list ap);
void               progress_window_set_size   (struct MuttWindow *win, size_t size);
bool               progress_window_update     (struct MuttWindow *win, size_t pos, int percent);

#endif /* MUTT_PROGRESS_WINDOW_H */
