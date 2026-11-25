/**
 * @file
 * Tabbed Container Window
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_GUI_TABBED_H
#define MUTT_GUI_TABBED_H

#include <stdbool.h>
#include "mutt/lib.h"

struct MuttWindow;

ARRAY_HEAD(WindowArray, struct MuttWindow *);
ARRAY_HEAD(StringArray2, const char *);

/**
 * struct TabWinWindowData - Tabbed Window private data
 */
struct TabWinWindowData
{
  int active_tab;   ///< One-based index of the active Tab
  bool wrap_around; ///< Wrap when selecting the next or previous Tab

  struct MuttWindow *tabbar; ///< Tab bar Window
  struct MuttWindow *cont;   ///< Container for the Tab Windows

  struct StringArray2 names; ///< Tab names
  struct WindowArray tabs;   ///< Tab Windows (not owned)
};

struct MuttWindow *tabwin_new(void);

/**
 * @defgroup tabbed_function_api Tabbed Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Tabbed Function
 *
 * @param win Tabbed Window
 * @param op     Operation to perform, e.g. OP_VERSION
 * @retval enum #FunctionRetval
 */
typedef int (*tabbed_function_t)(struct MuttWindow *win, int op);

/**
 * struct TabbedFunction - A NeoMutt function
 */
struct TabbedFunction
{
  int op;                     ///< Op code, e.g. OP_TABBED_NEXT
  tabbed_function_t function; ///< Function to call
};

int tabbed_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event);

bool tabwin_select_tab_first(struct MuttWindow *win);
bool tabwin_select_tab_last (struct MuttWindow *win);
bool tabwin_select_tab_num  (struct MuttWindow *win, int num);
bool tabwin_select_tab_next (struct MuttWindow *win);
bool tabwin_select_tab_prev (struct MuttWindow *win);

int tabwin_add_child(struct MuttWindow *win, const char *name, struct MuttWindow *child);

#endif /* MUTT_GUI_TABBED_H */
