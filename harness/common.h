/**
 * @file
 * Shared test harness for mailbox backends
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page lib_harness Backend Test Harnesses
 *
 * Shared test harness for mailbox backends
 *
 * Each source file in the library tests one of the Mailbox backends.
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | harness/common.c    | @subpage harness_common    |
 * | harness/compmbox.c  | @subpage harness_compmbox  |
 * | harness/imap.c      | @subpage harness_imap      |
 * | harness/maildir.c   | @subpage harness_maildir   |
 * | harness/mbox.c      | @subpage harness_mbox      |
 * | harness/mh.c        | @subpage harness_mh        |
 * | harness/mmdf.c      | @subpage harness_mmdf      |
 * | harness/modules.c   | @subpage harness_modules   |
 * | harness/nntp.c      | @subpage harness_nntp      |
 * | harness/notmuch.c   | @subpage harness_notmuch   |
 * | harness/pop.c       | @subpage harness_pop       |
 */

#ifndef MUTT_HARNESS_COMMON_H
#define MUTT_HARNESS_COMMON_H

#include <stdbool.h>
#include "core/lib.h"

/**
 * struct HarnessOpts - Parsed command-line options for a harness program
 */
struct HarnessOpts
{
  enum MailboxType type;      ///< Mailbox type to force (MUTT_UNKNOWN = auto-detect)
  const char      *path;      ///< Mailbox path
  bool             list;      ///< List emails
  int              read_num;  ///< Email number to read (-1 = none)
  bool             check;     ///< Check for new mail
  int              repeat;    ///< Number of iterations (for benchmarking)
  bool             quiet;     ///< Suppress output
  bool             verbose;   ///< Extra debug output
  const char      *user;      ///< Username for network backends
  const char      *pass;      ///< Password for network backends
};

bool harness_init      (const struct Module **modules, bool quiet);
void harness_cleanup   (void);
int  harness_parse_args(struct HarnessOpts *opts, int argc, char *argv[]);
int  harness_run       (struct HarnessOpts *opts);

extern const struct Module *Modules[];

#endif /* MUTT_HARNESS_COMMON_H */
