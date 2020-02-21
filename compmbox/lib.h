/**
 * @file
 * Compressed mbox local mailbox type
 *
 * @authors
 * Copyright (C) 1997 Alain Penders <Alain@Finale-Dev.com>
 * Copyright (C) 2016 Richard Russon <rich@flatcap.org>
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
 * @page compmbox COMPMBOX: Compressed Mailbox
 *
 * Compressed mbox local mailbox type
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | compress/compress.c | @subpage compmbox_compress |
 */

#ifndef MUTT_COMPMBOX_LIB_H
#define MUTT_COMPMBOX_LIB_H

#include <stdbool.h>
#include <stdio.h>
#include "mx.h"
#include "path.h"

struct Mailbox;

/**
 * struct CompressInfo - Private data for compress
 *
 * This object gets attached to the Mailbox.
 */
struct CompressInfo
{
  const char *cmd_append;        ///< append-hook command
  const char *cmd_close;         ///< close-hook  command
  const char *cmd_open;          ///< open-hook   command
  long size;                     ///< size of the compressed file
  const struct MxOps *child_ops; ///< callbacks of de-compressed file
  bool locked;                   ///< if realpath is locked
  FILE *fp_lock;                 ///< fp used for locking
};

bool mutt_comp_can_append(struct Mailbox *m);
bool mutt_comp_can_read(const char *path);
int  mutt_comp_valid_command(const char *cmd);

extern struct MxOps MxCompOps;

#endif /* MUTT_COMPMBOX_LIB_H */
