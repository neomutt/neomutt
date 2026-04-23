/**
 * @file
 * Utility Window
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

#ifndef MUTT_GUI_UTILWIN_H
#define MUTT_GUI_UTILWIN_H

#include "config/lib.h"

/**
 * enum KeyPreviewPosition - Position of the key preview window
 */
enum KeyPreviewPosition
{
  KEY_PREVIEW_LEFT,  ///< Key preview on the left
  KEY_PREVIEW_RIGHT, ///< Key preview on the right
};

extern const struct EnumDef KeyPreviewPositionDef;

struct MuttWindow;

struct MuttWindow *utilwin_new     (void);
void               utilwin_set_text(struct MuttWindow *win, const char *text);

#endif /* MUTT_GUI_UTILWIN_H */
