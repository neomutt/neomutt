/**
 * @file
 * GUI display the mailboxes in a side panel
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

/**
 * @page lib_attach Attachments
 *
 * Attachment handling
 *
 * | File                  | Description                  |
 * | :-------------------- | :--------------------------- |
 * | attach/attach.c       | @subpage attach_attach       |
 * | attach/attachments.c  | @subpage attach_attachments  |
 * | attach/dlg_attach.c   | @subpage attach_dlg_attach   |
 * | attach/functions.c    | @subpage attach_functions    |
 * | attach/lib.c          | @subpage attach_lib          |
 * | attach/mutt_attach.c  | @subpage attach_mutt_attach  |
 * | attach/private_data.c | @subpage attach_private_data |
 * | attach/recvattach.c   | @subpage attach_recvattach   |
 */

#ifndef MUTT_ATTACH_LIB_H
#define MUTT_ATTACH_LIB_H

// IWYU pragma: begin_exports
#include "attach.h"
#include "attachments.h"
#include "mutt_attach.h"
#include "recvattach.h"
// IWYU pragma: end_exports


struct Body;

int  attach_body_count   (struct Body *body, bool recurse);
bool attach_body_parent  (struct Body *start, struct Body *start_parent,
                          struct Body *body, struct Body **body_parent);
bool attach_body_previous(struct Body *start, struct Body *body,
                          struct Body **previous);

#endif /* MUTT_ATTACH_LIB_H */
