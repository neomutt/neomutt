/**
 * @file
 * Window state data for the Simple Pager
 *
 * @authors
 * Copyright (C) 2024-2025 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_SPAGER_WDATA_H
#define MUTT_SPAGER_WDATA_H

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "pfile/lib.h"
#include "search.h"

struct MuttWindow;

/**
 * SpagerWrapFlags - Simple Pager Wrapping flags
 */
typedef uint8_t SpagerWrapFlags;    ///< Flags, e.g. #SPW_POSITIVE
#define SPW_NO_FLAGS            0   ///< No flags are set
#define SPW_POSITIVE      (1 << 0)  ///< Wrap width measures distance from left-hand-side
#define SPW_NEGATIVE      (1 << 1)  ///< Wrap width measures distance from right-hand-side
#define SPW_MARKERS       (1 << 2)  ///< Show markers before wrapped text
#define SPW_SMART         (1 << 3)  ///< Perform smart wrapper (at word boundaries)
#define SPW_NO_WRAP       (1 << 4)  ///< Do not wrap text

/**
 * enum SpagerTristate - Tri-state for features
 */
enum SpagerTristate
{
  SPT_ENABLED = 1,       ///< Feature is always enabled
  SPT_DISABLED,          ///< Feature is always disabled
  SPT_CONFIG,            ///< Feature is controlled by config settings
};

/**
 * struct EventSimplePager - An Event that happened to a SimplePager
 */
struct EventSimplePager
{
  struct MuttWindow *win;             ///< The SimplePager this Event relates to
};

/**
 * enum NotifySimplePager - Simple Pager notification types
 *
 * Observers of #NT_SPAGER will be passed an #EventSimplePager.
 *
 * @note Notifications are sent **after** the event.
 */
enum NotifySimplePager
{
  NT_SPAGER_COLOR = 1,                ///< Simple Pager colour has changed
  NT_SPAGER_MOVE,                     ///< Simple Pager view has moved
  NT_SPAGER_SEARCH,                   ///< Simple Pager search has changed
  NT_SPAGER_TEXT,                     ///< Simple Pager text has changed
};

/**
 * struct SimplePagerExport - Convenience wrapper for exported data
 */
struct SimplePagerExport
{
  int num_rows;                       ///< Number of real rows
  int num_vrows;                      ///< Number of virtual rows (including wrapping)
  int top_row;                        ///< Top real row visible
  int top_vrow;                       ///< Top virtual row visible
  int bytes_pos;                      ///< Byte offset in the file
  int bytes_total;                    ///< Size of the file in bytes
  int percentage;                     ///< Percentage though the file
  int search_rows;                    ///< How many rows contain search matches
  int search_matches;                 ///< How many search matches in total
  enum SearchDirection direction;     ///< Search direction
};

/**
 * struct ViewRow - XXX
 */
struct ViewRow
{
  const char *text;                       ///< Plain text
  struct PagedTextMarkupArray markup;     ///< Markup for the text
  int num_bytes;                          ///< Number of bytes in the text
  int num_cols;                           ///< Number of screen columns used by the text
  struct SegmentArray segments;           ///< Lengths of wrapped parts of the Row
};
ARRAY_HEAD(ViewRowArray, struct ViewRow *);

/**
 * struct SimplePagerWindowData - Window state data for the Simple Pager
 */
struct SimplePagerWindowData
{
  struct PagedFile *paged_file;       ///< Parent PagedFile
  struct ConfigSubset *sub;           ///< Config

  short c_wrap;                       ///< Cached copy of $wrap
  bool c_markers    : 1;              ///< Cached copy of $markers
  bool c_smart_wrap : 1;              ///< Cached copy of $smart_wrap
  bool c_tilde      : 1;              ///< Cached copy of $tilde

  struct SimplePagerSearch *search;   ///< Search data

  int page_rows;                      ///< Cached copy of Window height
  int page_cols;                      ///< Cached copy of Window width

  int vrow;                           ///< Virtual Row at the top of the view

  struct Notify *notify;              ///< Notifications: #NotifySimplePager

  struct ViewRowArray vrows;          ///< XXX

  int scr_row;                        ///< Screen position
  int scr_col;                        ///< XXX
  int scr_off;                        ///< XXX

  // Effectively the cursor; the top Row visible in the View
  int top_row;                        ///< View top is this Row
  int top_col;                        ///< View top is this column of the Row
  int top_off;                        ///< View top if this byte offset into the Row
};

void                          spager_wdata_free(struct MuttWindow *win, void **ptr);
struct SimplePagerWindowData *spager_wdata_new (void);

bool spager_observer_add   (struct MuttWindow *win, observer_t callback, void *global_data);
bool spager_observer_remove(struct MuttWindow *win, observer_t callback, void *global_data);

void spager_get_data(struct MuttWindow *win, struct SimplePagerExport *spe);

#endif /* MUTT_SPAGER_WDATA_H */
