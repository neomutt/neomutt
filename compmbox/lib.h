/**
 * @file
 * Compressed mbox local mailbox type
 *
 * @authors
 * Copyright (C) 2017-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
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
 * @page lib_compmbox Compressed Mailbox
 *
 * Compressed mbox local mailbox type
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | compmbox/compress.c | @subpage compmbox_compress |
 * | compmbox/expando.c  | @subpage compmbox_expando  |
 * | compmbox/module.c   | @subpage compmbox_module   |
 */

#ifndef MUTT_COMPMBOX_LIB_H
#define MUTT_COMPMBOX_LIB_H

#include <stdbool.h>
#include <stdio.h>

struct Mailbox;

/**
 * ExpandoDataCompress - Expando UIDs for Compression
 *
 * @sa ED_COMPRESS, ExpandoDomain
 */
enum ExpandoDataCompress
{
  ED_CMP_FROM = 1,  ///< 'from' path
  ED_CMP_TO,        ///< 'to'   path
};

/**
 * struct CompressInfo - Private data for compress
 *
 * This object gets attached to the Mailbox.
 */
struct CompressInfo
{
  struct Expando *cmd_append;    ///< append-hook command
  struct Expando *cmd_close;     ///< close-hook  command
  struct Expando *cmd_open;      ///< open-hook   command
  long size;                     ///< size of the compressed file
  const struct MxOps *child_ops; ///< callbacks of de-compressed file
  bool locked;                   ///< if realpath is locked
  FILE *fp_lock;                 ///< fp used for locking
};

bool mutt_comp_can_append(struct Mailbox *m);
bool mutt_comp_can_read(const char *path);
int  mutt_comp_valid_command(const char *cmd);

extern const struct MxOps MxCompOps;

#endif /* MUTT_COMPMBOX_LIB_H */
