/**
 * @file
 * Shared constants/structs that are private to libconn
 *
 * @authors
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_NCRYPT_PRIVATE_H
#define MUTT_NCRYPT_PRIVATE_H

#include "config.h"
#include <stdbool.h>
#include <stdio.h>

struct Address;
struct CryptKeyInfo;
struct PgpKeyInfo;
struct SmimeKey;

/**
 * struct PgpEntry - An entry in a PGP key menu
 */
struct PgpEntry
{
  size_t num; ///< Index number
  struct PgpUid *uid;
};

struct CryptKeyInfo *dlg_gpgme(struct CryptKeyInfo *keys, struct Address *p, const char *s, unsigned int app, bool *forced_valid);
struct PgpKeyInfo *  dlg_pgp(struct PgpKeyInfo *keys, struct Address *p, const char *s);
struct SmimeKey *    dlg_smime(struct SmimeKey *keys, const char *query);

#endif /* MUTT_NCRYPT_PRIVATE_H */
