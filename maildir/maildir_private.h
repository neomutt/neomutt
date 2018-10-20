/**
 * @file
 * Maildir/MH private types
 *
 * @authors
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

#ifndef MUTT_MAILDIR_MAILDIR_PRIVATE_H
#define MUTT_MAILDIR_MAILDIR_PRIVATE_H

#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

/**
 * struct MaildirMboxData - MH-specific mailbox data
 */
struct MaildirMboxData
{
  struct timespec mtime_cur;
  mode_t mh_umask;
};

/**
 * struct Maildir - A Maildir mailbox
 */
struct Maildir
{
  struct Email *email;
  char *canon_fname;
  bool header_parsed : 1;
  ino_t inode;
  struct Maildir *next;
};

/**
 * struct MhSequences - Set of MH sequence numbers
 */
struct MhSequences
{
  int max;
  short *flags;
};

#endif /* MUTT_MAILDIR_MAILDIR_PRIVATE_H */
