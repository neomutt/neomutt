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

#ifndef MUTT_COMPRESS_H
#define MUTT_COMPRESS_H

#include <stdbool.h>
#include "mx.h"

struct Context;

/**
 * struct CompressInfo - Private data for compress
 *
 * This object gets attached to the Mailbox.
 */
struct CompressInfo
{
  const char *append;            /**< append-hook command */
  const char *close;             /**< close-hook  command */
  const char *open;              /**< open-hook   command */
  long size;                     /**< size of the compressed file */
  const struct MxOps *child_ops; /**< callbacks of de-compressed file */
  bool locked;                   /**< if realpath is locked */
  FILE *lockfp;                  /**< fp used for locking */
};

bool mutt_comp_can_append(struct Mailbox *m);
bool mutt_comp_can_read(const char *path);
int mutt_comp_valid_command(const char *cmd);
int comp_path_probe(const char *path, const struct stat *st);

extern struct MxOps mx_comp_ops;

#endif /* MUTT_COMPRESS_H */
