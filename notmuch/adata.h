/**
 * @file
 * Notmuch-specific Account data
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

#ifndef MUTT_NOTMUCH_ADATA_H
#define MUTT_NOTMUCH_ADATA_H

#include <notmuch.h>
#include <stdbool.h>

struct Mailbox;

/**
 * struct NmAccountData - Notmuch-specific Account data - @extends Account
 */
struct NmAccountData
{
  notmuch_database_t *db;
  bool longrun : 1;    ///< A long-lived action is in progress
  bool trans : 1;      ///< Atomic transaction in progress
};

void                  nm_adata_free(void **ptr);
struct NmAccountData *nm_adata_get (struct Mailbox *m);
struct NmAccountData *nm_adata_new (void);

#endif /* MUTT_NOTMUCH_ADATA_H */
