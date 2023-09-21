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

#include "mutt/lib.h"

struct MuttWindow;

#define MSGWIN_MAX_ROWS 3

/**
 * struct MwChar - Description of a single character
 *
 * Measure the dimensions of each character in MsgWinWindowData.text.
 * This allows us to wrap the text efficiently.
 */
struct MwChar
{
  unsigned char width;              ///< Width in screen cells
  unsigned char bytes;              ///< Number of bytes to represent
  const struct AttrColor *ac_color; ///< Colour to use
};
ARRAY_HEAD(MwCharArray, struct MwChar);

/**
 * struct MwChunk - A block of characters of one colour
 *
 * A chunk represents one colour of consecutive characters in one row.
 * If the colour changes, chunk is too wide to fit on the screen,
 * it will be split into multiple chunks.
 */
struct MwChunk
{
  unsigned short offset;            ///< Offset into MsgWinWindowData.text
  unsigned short bytes;             ///< Number of bytes in the row
  unsigned short width;             ///< Width of row in screen cells
  const struct AttrColor *ac_color; ///< Colour to use
};
ARRAY_HEAD(MwChunkArray, struct MwChunk);

/**
 * struct MsgWinWindowData - Message Window private Window data
 */
struct MsgWinWindowData
{
  struct Buffer *text;                       ///< Cached display string
  struct MwCharArray chars;                  ///< Text: Breakdown of bytes and widths
  struct MwChunkArray rows[MSGWIN_MAX_ROWS]; ///< String byte counts for each row
  int row;                                   ///< Cursor row
  int col;                                   ///< Cursor column
};

void                     msgwin_wdata_free(struct MuttWindow *win, void **ptr);
struct MsgWinWindowData *msgwin_wdata_new (void);

#endif /* MUTT_GUI_MSGWIN_WDATA_H */
