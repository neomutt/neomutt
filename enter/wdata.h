/**
 * @file
 * Enter Window Data
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

#ifndef MUTT_ENTER_WDATA_H
#define MUTT_ENTER_WDATA_H

#include "config.h"
#include <stdbool.h>
#include <wchar.h>
#include <wctype.h>
#include "mutt.h"
#include "history/lib.h"

struct MuttWindow;

/**
 * enum EnterRedrawFlags - Redraw flags for mutt_enter_string_full()
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
  char *buf;                      ///< Buffer for the result
  size_t buflen;                  ///< Length of result buffer
  int col;                        ///< Initial cursor positions
  CompletionFlags flags;          ///< Flags, see #CompletionFlags
  bool multiple;                  ///< Allow multiple matches
  struct Mailbox *m;              ///< Mailbox
  char ***files;                  ///< List of files selected
  int *numfiles;                  ///< Number of files selected
  struct EnterState *state;       ///< Current state of text entry

  // Local variables
  enum EnterRedrawFlags redraw;   ///< What needs redrawing? See #EnterRedrawFlags
  bool pass;                      ///< Password mode, conceal characters
  bool first;                     ///< First time through, no input yet
  enum HistoryClass hclass;       ///< History to use, e.g. #HC_COMMAND
  wchar_t *tempbuf;               ///< Buffer used by completion
  size_t templen;                 ///< Length of complete buffer
  mbstate_t *mbstate;             ///< Multi-byte state
  int tabs;                       ///< Number of times the user has hit tab

  bool done;                      ///< Is text-entry done?
};

#endif /* MUTT_ENTER_WDATA_H */
