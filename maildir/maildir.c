/**
 * @file
 * Maildir local mailbox type
 *
 * @authors
 * Copyright (C) 2018-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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
 * @page maildir_maildir Maildir local mailbox type
 *
 * Maildir local mailbox type
 *
 * Implementation: #MxMaildirOps
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "core/lib.h"
#include "account.h"
#include "mailbox.h"
#include "message.h"
#include "path.h"

/**
 * MxMaildirOps - Maildir Mailbox - Implements ::MxOps - @ingroup mx_api
 */
const struct MxOps MxMaildirOps = {
  // clang-format off
  .type            = MUTT_MAILDIR,
  .name             = "maildir",
  .is_local         = true,
  .ac_owns_path     = maildir_ac_owns_path,
  .ac_add           = maildir_ac_add,
  .mbox_open        = maildir_mbox_open,
  .mbox_open_append = maildir_mbox_open_append,
  .mbox_check       = maildir_mbox_check,
  .mbox_check_stats = maildir_mbox_check_stats,
  .mbox_sync        = maildir_mbox_sync,
  .mbox_close       = maildir_mbox_close,
  .msg_open         = maildir_msg_open,
  .msg_open_new     = maildir_msg_open_new,
  .msg_commit       = maildir_msg_commit,
  .msg_close        = maildir_msg_close,
  .msg_padding_size = NULL,
  .msg_save_hcache  = maildir_msg_save_hcache,
  .tags_edit        = NULL,
  .tags_commit      = NULL,
  .path_probe       = maildir_path_probe,
  .path_canon       = maildir_path_canon,
  .path_is_empty    = maildir_path_is_empty,
  // clang-format on
};
