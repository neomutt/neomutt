/**
 * @file
 * Maildir-specific Mailbox data
 *
 * @authors
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MAILDIR_MDATA_H
#define MUTT_MAILDIR_MDATA_H

#include "config.h"
#include <sys/types.h>
#include <time.h>

struct Mailbox;

/**
 * struct MaildirMboxData - Maildir-specific Mailbox data - @extends Mailbox
 */
struct MaildirMboxData
{
  struct timespec mtime;     ///< Time Mailbox was last changed
  struct timespec mtime_cur; ///< Timestamp of the 'cur' dir
  mode_t umask;              ///< umask to use when creating files
#ifdef USE_MONITOR
  int wd_new;                ///< Watch descriptor for the new/ directory
  int wd_cur;                ///< Watch descriptor for the cur/ directory
#endif
};

void                    maildir_mdata_free(void **ptr);
struct MaildirMboxData *maildir_mdata_get(struct Mailbox *m);
struct MaildirMboxData *maildir_mdata_new(const char *path);

#endif /* MUTT_MAILDIR_MDATA_H */
