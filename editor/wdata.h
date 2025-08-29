/**
 * @file
 * Enter Window Data
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

#ifndef MUTT_EDITOR_WDATA_H
#define MUTT_EDITOR_WDATA_H

#include "config.h"
#include <stdbool.h>
#include <wchar.h>
#include "mutt.h"
#include "history/lib.h"

/**
 * enum EnterRedrawFlags - Redraw flags for mw_get_field()
 */
enum EnterRedrawFlags
{
  ENTER_REDRAW_NONE = 0, ///< Nothing to redraw
  ENTER_REDRAW_INIT,     ///< Go to end of line and redraw
  ENTER_REDRAW_LINE,     ///< Redraw entire line
};

/**
 * struct EnterWindowData - Data to fill the Enter Window
 */
struct EnterWindowData
{
  // Function parameters
  struct Buffer *buffer;          ///< struct Buffer for the result
  CompletionFlags flags;          ///< Flags, see #CompletionFlags
  struct EnterState *state;       ///< Current state of text entry
  enum HistoryClass hclass;       ///< History to use, e.g. #HC_NEO_COMMAND
  const struct CompleteOps *comp_api; ///< Auto-Completion API
  void *cdata;                    ///< Auto-Completion private data

  // Local variables
  const char *prompt;             ///< Prompt
  enum EnterRedrawFlags redraw;   ///< What needs redrawing? See #EnterRedrawFlags
  bool pass;                      ///< Password mode, conceal characters
  bool first;                     ///< First time through, no input yet
  wchar_t *tempbuf;               ///< Buffer used by completion
  size_t templen;                 ///< Length of complete buffer
  mbstate_t *mbstate;             ///< Multi-byte state
  int tabs;                       ///< Number of times the user has hit tab

  bool done;                      ///< Is text-entry done?

  struct CompletionData *cd;      ///< Auto-completion state data

  int row;                        ///< Cursor row
  int col;                        ///< Cursor column
};

#endif /* MUTT_EDITOR_WDATA_H */
