/**
 * @file
 * Private state data for the Pager
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_PAGER_PRIVATE_DATA_H
#define MUTT_PAGER_PRIVATE_DATA_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "lib.h"
#include "color/lib.h"

struct MuttWindow;

/**
 * struct PagerPrivateData - Private state data for the Pager
 */
struct PagerPrivateData
{
  struct PagerView *pview;     ///< Object to view in the pager

  FILE *fp;                    ///< File containing decrypted/decoded/weeded Email
  struct stat st;              ///< Stats about Email file
  LOFF_T bytes_read;           ///< Number of bytes read from file

  struct Line *lines;          ///< Array of text lines in pager
  int lines_used;              ///< Size of lines array (used entries)
  int lines_max;               ///< Capacity of lines array (total entries)
  int cur_line;                ///< Current line (last line visible on screen)

  int old_top_line;            ///< Old top line, used for repainting
  int win_height;              ///< Number of lines in the Window
  int top_line;                ///< First visible line on screen
  int has_types;               ///< Set to MUTT_TYPES for PAGER_MODE_EMAIL or MUTT_SHOWCOLOR

  struct QuoteStyle *quote_list;///< Tree of quoting levels
  int q_level;                 ///< Number of unique quoting levels
  PagerFlags hide_quoted;      ///< Set to MUTT_HIDE when quoted email is hidden `<toggle-quoted>`

  PagerFlags search_flag;      ///< Set to MUTT_SEARCH when search results are visible `<search-toggle>`
  char search_str[256];        ///< Current search string
  bool search_compiled;        ///< Search regex is in use
  regex_t search_re;           ///< Compiled search string
  bool search_back;            ///< Search backwards

  bool force_redraw;              ///< Repaint is needed
  PagerRedrawFlags redraw;        ///< When to redraw the screen
  struct AttrColorList ansi_list; ///< List of ANSI colours used in the Pager
  struct Notify *notify;          ///< Notifications: #NotifyPager, #PagerPrivateData

  int rc;                        ///< Return code from functions
  int searchctx;                 ///< Space to show around search matches
  bool first;                    ///< First time flag for toggle-new
  bool wrapped;                  ///< Has the search/next wrapped around?
  uint64_t delay_read_timestamp; ///< Time that email was first shown
  bool pager_redraw;             ///< Force a complete redraw
  enum PagerLoopMode loop;       ///< What the Event Loop should do next, e.g. #PAGER_LOOP_CONTINUE
};

void                     pager_private_data_free(struct MuttWindow *win, void **ptr);
struct PagerPrivateData *pager_private_data_new (void);

#endif /* MUTT_PAGER_PRIVATE_DATA_H */
