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
#include <stdio.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "lib.h"
#include "menu/lib.h"

struct MuttWindow;

/**
 * struct PagerPrivateData - Private state data for the Pager
 */
struct PagerPrivateData
{
  struct Menu *menu;
  struct MuttWindow *win_pbar;

  struct PagerView *pview;
  int indexlen;
  int indicator; ///< the indicator line of the PI
  int oldtopline;
  int lines;
  int max_line;
  int last_line;
  int curline;
  int topline;
  bool force_redraw;
  int has_types;
  PagerFlags hide_quoted;
  int q_level;
  struct QClass *quote_list;
  LOFF_T last_pos;
  LOFF_T last_offset;
  regex_t search_re;
  bool search_compiled;
  PagerFlags search_flag;
  bool search_back;
  char searchbuf[256];
  struct Line *line_info;
  FILE *fp;
  struct stat sb;

  MenuRedrawFlags redraw; ///< When to redraw the screen
};

void                     pager_private_data_free(struct MuttWindow *win, void **ptr);
struct PagerPrivateData *pager_private_data_new (void);

#endif /* MUTT_PAGER_PRIVATE_DATA_H */
