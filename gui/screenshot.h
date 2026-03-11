/**
 * @file
 * Screenshot Capture
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_GUI_SCREENSHOT_H
#define MUTT_GUI_SCREENSHOT_H

#include "config.h"
#include <stdbool.h>
#include "color/stack.h"

struct ScreenshotState;

extern struct ScreenshotState *ScreenshotActive;

struct ScreenshotState *screenshot_new           (int rows, int cols);
bool                    screenshot_end_and_write  (struct ScreenshotState **pss, const char *path);
void                    screenshot_move_cursor    (struct ScreenshotState *ss, int row, int col);
void                    screenshot_set_color_stack(struct ScreenshotState *ss, const struct ColorStack *cs);
void                    screenshot_write_text     (struct ScreenshotState *ss, const char *str, int bytes);

#endif /* MUTT_GUI_SCREENSHOT_H */
