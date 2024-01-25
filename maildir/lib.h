/**
 * @file
 * Maildir local mailbox type
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page lib_maildir Maildir Mailbox
 *
 * Maildir local mailbox type
 *
 * | File               | Description               |
 * | :----------------- | :------------------------ |
 * | maildir/account.c  | @subpage maildir_account  |
 * | maildir/config.c   | @subpage maildir_config   |
 * | maildir/edata.c    | @subpage maildir_edata    |
 * | maildir/hcache.c   | @subpage maildir_hcache   |
 * | maildir/mailbox.c  | @subpage maildir_mailbox  |
 * | maildir/maildir.c  | @subpage maildir_maildir  |
 * | maildir/mdata.c    | @subpage maildir_mdata    |
 * | maildir/mdemail.c  | @subpage maildir_mdemail  |
 * | maildir/message.c  | @subpage maildir_message  |
 * | maildir/path.c     | @subpage maildir_path     |
 * | maildir/shared.c   | @subpage maildir_shared   |
 */

#ifndef MUTT_MAILDIR_LIB_H
#define MUTT_MAILDIR_LIB_H

#include "core/lib.h"

extern const struct MxOps MxMaildirOps;

#endif /* MUTT_MAILDIR_LIB_H */
