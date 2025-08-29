/**
 * @file
 * Mh-specific Mailbox data
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MH_MDATA_H
#define MUTT_MH_MDATA_H

#include <sys/types.h>
#include <time.h>

struct Mailbox;

/**
 * struct MhMboxData - Mh-specific Mailbox data - @extends Mailbox
 */
struct MhMboxData
{
  struct timespec mtime;     ///< Time Mailbox was last changed
  struct timespec mtime_seq; ///< Time '.mh_sequences' was last changed
  mode_t          umask;     ///< umask to use when creating files
};

void               mh_mdata_free(void **ptr);
struct MhMboxData *mh_mdata_get(struct Mailbox *m);
struct MhMboxData *mh_mdata_new(void);

#endif /* MUTT_MH_MDATA_H */
