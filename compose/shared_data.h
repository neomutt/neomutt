/**
 * @file
 * Compose Shared Data
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

#ifndef MUTT_COMPOSE_SHARED_DATA_H
#define MUTT_COMPOSE_SHARED_DATA_H

#include "config.h"
#include <stdbool.h>

/**
 * struct ComposeSharedData - Shared Compose Data
 */
struct ComposeSharedData
{
  struct ConfigSubset *sub;          ///< Config set to use
  struct Mailbox *mailbox;           ///< Current Mailbox
  struct Email *email;               ///< Email being composed
  struct ComposeAttachData *adata;   ///< Attachments
  struct ComposeEnvelopeData *edata; ///< Envelope data
  struct Notify *notify;             ///< Notifications: #NotifyCompose

  struct Buffer *fcc;                ///< Buffer to save FCC
  int flags;                         ///< Flags, e.g. #MUTT_COMPOSE_NOFREEHEADER
  bool fcc_set;                      ///< User has edited the Fcc: field
  int rc;                            ///< Return code to leave compose
#ifdef USE_NNTP
  bool news;                         ///< Email is a news article
#endif
};

struct MuttWindow;

void compose_shared_data_free(struct MuttWindow *win, void **ptr);
struct ComposeSharedData *compose_shared_data_new(void);

#endif /* MUTT_COMPOSE_SHARED_DATA_H */
