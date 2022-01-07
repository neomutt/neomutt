/**
 * @file
 * GUI display the mailboxes in a side panel
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_SIDEBAR_PRIVATE_H
#define MUTT_SIDEBAR_PRIVATE_H

#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "color/lib.h"

struct IndexSharedData;
struct Mailbox;
struct MuttWindow;

extern struct ListHead SidebarWhitelist;

/**
 * struct SbEntry - Info about folders in the sidebar
 */
struct SbEntry
{
  char box[256];           ///< Mailbox path (possibly abbreviated)
  char display[256];       ///< Formatted string to display
  int depth;               ///< Indentation depth
  struct Mailbox *mailbox; ///< Mailbox this represents
  bool is_hidden;          ///< Don't show, e.g. $sidebar_new_mail_only
  enum ColorId cid;        ///< Colour to use
};
ARRAY_HEAD(SbEntryArray, struct SbEntry *);

/**
 * enum DivType - Source of the sidebar divider character
 */
enum DivType
{
  SB_DIV_USER,  ///< User configured using $sidebar_divider_char
  SB_DIV_ASCII, ///< An ASCII vertical bar (pipe)
  SB_DIV_UTF8,  ///< A unicode line-drawing character
};

/**
 * struct SidebarWindowData - Sidebar private Window data - @extends MuttWindow
 */
struct SidebarWindowData
{
  struct IndexSharedData *shared;         ///< Shared Index Data
  struct SbEntryArray entries;            ///< Items to display in the sidebar

  int top_index;             ///< First mailbox visible in sidebar
  int opn_index;             ///< Current (open) mailbox
  int hil_index;             ///< Highlighted mailbox
  int bot_index;             ///< Last mailbox visible in sidebar

  short previous_sort;       ///< Old `$sidebar_sort_method`
  enum DivType divider_type; ///< Type of divider to use, e.g. #SB_DIV_ASCII
  short divider_width;       ///< Width of the divider in screen columns
};

// sidebar.c
void sb_add_mailbox        (struct SidebarWindowData *wdata, struct Mailbox *m);
void sb_remove_mailbox     (struct SidebarWindowData *wdata, struct Mailbox *m);
void sb_set_current_mailbox(struct SidebarWindowData *wdata, struct Mailbox *m);

// functions.c
bool select_next(struct SidebarWindowData *wdata);
bool select_prev(struct SidebarWindowData *wdata);

// observer.c
int sb_insertion_window_observer(struct NotifyCallback *nc);
void sb_win_add_observers(struct MuttWindow *win);

// sort.c
void sb_sort_entries(struct SidebarWindowData *wdata, enum SortType sort);

// wdata.c
void                      sb_wdata_free(struct MuttWindow *win, void **ptr);
struct SidebarWindowData *sb_wdata_get(struct MuttWindow *win);
struct SidebarWindowData *sb_wdata_new(struct IndexSharedData *shared);

// window.c
int sb_recalc(struct MuttWindow *win);
int sb_repaint(struct MuttWindow *win);

#endif /* MUTT_SIDEBAR_PRIVATE_H */
