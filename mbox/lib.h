/**
 * @file
 * Mbox local mailbox type
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
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
 * @page lib_mbox Mbox
 *
 * Mbox local mailbox type
 *
 * | File          | Description          |
 * | :------------ | :------------------- |
 * | mbox/config.c | @subpage mbox_config |
 * | mbox/mbox.c   | @subpage mbox_mbox   |
 */

#ifndef MUTT_MBOX_LIB_H
#define MUTT_MBOX_LIB_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include "core/lib.h"

struct stat;

/**
 * struct MboxAccountData - Mbox-specific Account data - @extends Account
 */
struct MboxAccountData
{
  FILE *fp;                           ///< Mailbox file
  struct timespec mtime;              ///< Time Mailbox was last changed
  struct timespec atime;              ///< File's last-access time
  struct timespec stats_last_checked; ///< Mtime of mailbox the last time stats where checked

  bool locked : 1; ///< is the mailbox locked?
  bool append : 1; ///< mailbox is opened in append mode
};

extern const struct MxOps MxMboxOps;
extern const struct MxOps MxMmdfOps;

#define MMDF_SEP "\001\001\001\001\n"

enum MxStatus    mbox_check(struct Mailbox *m, struct stat *st, bool check_stats);
enum MailboxType mbox_path_probe(const char *path, const struct stat *st);
void             mbox_reset_atime(struct Mailbox *m, struct stat *st);

#endif /* MUTT_MBOX_LIB_H */
