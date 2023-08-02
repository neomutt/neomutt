/**
 * @file
 * Message Window private data
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_GUI_MSGWIN_WDATA_H
#define MUTT_GUI_MSGWIN_WDATA_H

#include "color/lib.h"

struct MuttWindow;

/**
 * struct MsgWinWindowData - Message Window private Window data
 */
struct MsgWinWindowData
{
  enum ColorId cid; ///< Colour for the text, e.g. #MT_COLOR_MESSAGE
  char *text;       ///< Cached display string
};

void                     msgwin_wdata_free(struct MuttWindow *win, void **ptr);
struct MsgWinWindowData *msgwin_wdata_new (void);

#endif /* MUTT_GUI_MSGWIN_WDATA_H */
