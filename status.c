/**
 * @file
 * GUI display a user-configurable status line
 *
 * @authors
 * Copyright (C) 1996-2000,2007 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2017-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018 Austin Ray <austin@austinray.io>
 * Copyright (C) 2020-2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2021 Eric Blake <eblake@redhat.com>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
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

/**
 * @page neo_status GUI display a user-configurable status line
 *
 * GUI display a user-configurable status line
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "status.h"
#include "expando/lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "postpone/lib.h"
#include "format_flags.h"
#include "globals.h"
#include "mutt_mailbox.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "mview.h"

/**
 * get_sort_str - Get the sort method as a string
 * @param buf    Buffer for the sort string
 * @param buflen Length of the buffer
 * @param method Sort method, see #SortType
 * @retval ptr Buffer pointer
 */
static char *get_sort_str(char *buf, size_t buflen, enum SortType method)
{
  snprintf(buf, buflen, "%s%s%s", (method & SORT_REVERSE) ? "reverse-" : "",
           (method & SORT_LAST) ? "last-" : "",
           mutt_map_get_name(method & SORT_MASK, SortMethods));
  return buf;
}

/**
 * struct MenuStatusLineData - Data for creating a Menu line
 */
struct MenuStatusLineData
{
  struct IndexSharedData *shared; ///< Data shared between Index, Pager and Sidebar
  struct Menu *menu;              ///< Current Menu
};

/**
 * menu_status_line - Create the status line
 * @param[out] buf      Buffer in which to save string
 * @param[in]  shared   Shared Index data
 * @param[in]  menu     Current menu
 * @param[in]  cols     Maximum number of columns to use
 * @param[in]  fmt      Format string
 *
 * @sa status_format_str()
 */
void menu_status_line(struct Buffer *buf, struct IndexSharedData *shared,
                      struct Menu *menu, int cols, const char *fmt)
{
  struct MenuStatusLineData data = { shared, menu };

  // mutt_expando_format(buf->data, buf->dsize, 0, cols, fmt, status_format_str,
  //                     (intptr_t) &data, MUTT_FORMAT_NO_FLAGS);
}
