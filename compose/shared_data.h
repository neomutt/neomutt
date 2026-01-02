/**
 * @file
 * Compose Shared Data
 *
 * @authors
 * Copyright (C) 2021-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2024 Dennis Schön <mail@dennis-schoen.de>
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

#ifndef MUTT_COMPOSE_SHARED_DATA_H
#define MUTT_COMPOSE_SHARED_DATA_H

#include <stdbool.h>

struct MuttWindow;

/**
 * struct ComposeSharedData - Shared Compose Data
 */
struct ComposeSharedData
{
  struct ConfigSubset *sub;              ///< Config set to use
  struct Mailbox *mailbox;               ///< Current Mailbox
  struct Email *email;                   ///< Email being composed
  struct ComposeAttachData *adata;       ///< Attachments
  struct MuttWindow *win_attach_bar;     ///< Status bar divider above attachments
  struct MuttWindow *win_preview;        ///< Message preview window
  struct MuttWindow *win_preview_bar;    ///< Status bar divider above preview

  struct Buffer *fcc;                    ///< Buffer to save FCC
  int flags;                             ///< Flags, e.g. #MUTT_COMPOSE_NOFREEHEADER
  bool fcc_set;                          ///< User has edited the Fcc: field
  int rc;                                ///< Return code to leave compose
};

/**
 * ExpandoDataCompose - Expando UIDs for Compose
 *
 * @sa ED_COMPOSE, ExpandoDomain
 */
enum ExpandoDataCompose
{
  ED_COM_ATTACH_COUNT = 1,     ///< ComposeAttachData, num_attachments()
  ED_COM_ATTACH_SIZE,          ///< ComposeAttachData, cum_attachs_size()
};

void compose_shared_data_free(struct MuttWindow *win, void **ptr);
struct ComposeSharedData *compose_shared_data_new(void);

#endif /* MUTT_COMPOSE_SHARED_DATA_H */
