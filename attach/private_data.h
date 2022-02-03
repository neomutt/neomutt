/**
 * @file
 * Private state data for Attachments
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_ATTACH_PRIVATE_DATA_H
#define MUTT_ATTACH_PRIVATE_DATA_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "lib.h"

struct Menu;

/**
 * struct AttachPrivateData - Private state data for Attachments
 */
struct AttachPrivateData
{
  struct Menu *menu;        ///< Current Menu
  struct AttachCtx *actx;   ///< List of all Attachments
  struct ConfigSubset *sub; ///< Config subset
  struct Mailbox *mailbox;  ///< Current Mailbox
  int op;                   ///< Op returned from the Pager, e.g. OP_NEXT_ENTRY
};

void                      attach_private_data_free(struct Menu *menu, void **ptr);
struct AttachPrivateData *attach_private_data_new (void);

#endif /* MUTT_ATTACH_PRIVATE_DATA_H */
