/**
 * @file
 * Notmuch-specific Mailbox data
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

#ifndef MUTT_NOTMUCH_MDATA_H
#define MUTT_NOTMUCH_MDATA_H

#include <stdbool.h>
#include "progress/lib.h"
#include "query.h" // IWYU pragma: keep

struct Mailbox;

/**
 * struct NmMboxData - Notmuch-specific Mailbox data - @extends Mailbox
 */
struct NmMboxData
{
  struct Url *db_url;          ///< Parsed view url of the Notmuch database
  char *db_query;              ///< Previous query
  int db_limit;                ///< Maximum number of results to return
  enum NmQueryType query_type; ///< Messages or Threads

  struct Progress progress;    ///< A progress bar
  int oldmsgcount;
  int ignmsgcount;             ///< Ignored messages

  bool noprogress : 1;         ///< Don't show the progress bar
  bool progress_ready : 1;     ///< A progress bar has been initialised
};

void                  nm_mdata_free(void **ptr);
struct NmMboxData *   nm_mdata_get (struct Mailbox *m);
struct NmMboxData *   nm_mdata_new (const char *url);

#endif /* MUTT_NOTMUCH_MDATA_H */
