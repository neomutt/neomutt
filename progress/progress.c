/**
 * @file
 * Progress bar
 *
 * @authors
 * Copyright (C) 2018-2022 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page progress_progress Progress Bar
 *
 * This is a wrapper around the Progress Bar Window.
 * After creating the window, it pushes it into the Message Window Container.
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "mutt_logging.h"
#include "options.h"
#include "window.h"

struct Progress;

/**
 * choose_increment - Choose the right increment given a ProgressType
 * @param type ProgressType
 * @retval Increment value
 */
static size_t choose_increment(enum ProgressType type)
{
  const short c_read_inc = cs_subset_number(NeoMutt->sub, "read_inc");
  const short c_write_inc = cs_subset_number(NeoMutt->sub, "write_inc");
  const short c_net_inc = cs_subset_number(NeoMutt->sub, "net_inc");
  const short *incs[] = { &c_read_inc, &c_write_inc, &c_net_inc };
  return (type >= mutt_array_size(incs)) ? 0 : *incs[type];
}

/**
 * progress_update - Update the state of the progress bar
 * @param progress Progress bar
 * @param pos      Position, or count
 * @param percent  Percentage complete
 * @retval true Screen update is needed
 *
 * If percent is -1, then the percentage will be calculated using pos and the
 * size in progress.
 *
 * If percent is positive, it is displayed as percentage, otherwise
 * percentage is calculated from size and pos if progress
 * was initialized with positive size, otherwise no percentage is shown
 */
bool progress_update(struct Progress *progress, size_t pos, int percent)
{
  // Decloak an opaque pointer
  struct MuttWindow *win = (struct MuttWindow *) progress;
  const bool updated = progress_window_update(win, pos, percent);
  window_redraw(win);
  return updated;
}

/**
 * progress_free - Free a Progress Bar
 * @param ptr Progress Bar to free
 */
void progress_free(struct Progress **ptr)
{
  if (!ptr)
    return;

  if (!*ptr)
  {
    mutt_clear_error();
    return;
  }

  // Decloak an opaque pointer
  struct MuttWindow **wptr = (struct MuttWindow **) ptr;
  struct MuttWindow *win_pop = msgcont_pop_window();
  if (win_pop != *wptr)
    return;

  mutt_window_free(wptr);
}

/**
 * progress_new - Create a new Progress Bar
 * @param msg  Message to display
 * @param type Type, e.g. #MUTT_PROGRESS_READ
 * @param size Total size of expected file / traffic
 * @retval ptr New Progress Bar
 *
 * If the user has disabled the progress bar, e.g. `set read_inc = 0` then a
 * simple message will be displayed instead.
 *
 * @note msg will be copied
 */
struct Progress *progress_new(const char *msg, enum ProgressType type, size_t size)
{
  if (OptNoCurses)
    return NULL;

  const size_t size_inc = choose_increment(type);
  if (size_inc == 0) // The user has disabled the progress bar
  {
    mutt_message(msg);
    return NULL;
  }

  const short c_time_inc = cs_subset_number(NeoMutt->sub, "time_inc");
  const bool is_bytes = (type == MUTT_PROGRESS_NET);

  struct MuttWindow *win = progress_window_new(msg, size, size_inc, c_time_inc, is_bytes);

  msgcont_push_window(win);

  // Return an opaque pointer
  return (struct Progress *) win;
}
