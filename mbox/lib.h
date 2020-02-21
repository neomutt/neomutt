/**
 * @file
 * Mbox local mailbox type
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page mbox MBOX: Local mailbox type
 *
 * Mbox local mailbox type
 *
 * | File        | Description        |
 * | :---------- | :----------------- |
 * | mbox/mbox.c | @subpage mbox_mbox |
 * | mbox/path.c | @subpage mbox_path |
 */

#ifndef MUTT_MBOX_LIB_H
#define MUTT_MBOX_LIB_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include "core/lib.h"
#include "mx.h"
#include "path.h"

struct stat;

/**
 * struct MboxAccountData - Mbox-specific Account data - @extends Account
 */
struct MboxAccountData
{
  FILE *fp;              ///< Mailbox file
  struct timespec atime; ///< File's last-access time

  bool locked : 1; ///< is the mailbox locked?
  bool append : 1; ///< mailbox is opened in append mode
};

extern struct MxOps MxMboxOps;
extern struct MxOps MxMmdfOps;

#define MMDF_SEP "\001\001\001\001\n"

int              mbox_check(struct Mailbox *m, struct stat *sb, bool check_stats);
enum MailboxType mbox_path_probe(const char *path, const struct stat *st);
void             mbox_reset_atime(struct Mailbox *m, struct stat *st);
bool             mbox_test_new_folder(const char *path);

#endif /* MUTT_MBOX_LIB_H */
