/**
 * @file
 * Misc PGP helper routines
 *
 * @authors
 * Copyright (C) 1996-1997 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2002 Thomas Roessler <roessler@does-not-exist.org>
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

#ifndef MUTT_NCRYPT_PGPLIB_H
#define MUTT_NCRYPT_PGPLIB_H

#include <stdbool.h>
#include <time.h>
#include "lib.h"

/**
 * struct PgpUid - PGP User ID
 */
struct PgpUid
{
  char *addr;
  short trust;
  int flags;
  struct PgpKeyInfo *parent;
  struct PgpUid *next;
};

/**
 * struct PgpKeyInfo - Information about a PGP key
 */
struct PgpKeyInfo
{
  char *keyid;
  char *fingerprint;
  struct PgpUid *address;
  KeyFlags flags;
  short keylen;
  time_t gen_time;
  int numalg;
  const char *algorithm;
  struct PgpKeyInfo *parent;
  struct PgpKeyInfo *next;
};

const char *pgp_pkalgbytype(unsigned char type);

struct PgpUid *pgp_copy_uids(struct PgpUid *up, struct PgpKeyInfo *parent);

bool pgp_canencrypt(unsigned char type);
bool pgp_cansign(unsigned char type);
short pgp_get_abilities(unsigned char type);

void pgp_key_free(struct PgpKeyInfo **kpp);

struct PgpKeyInfo *pgp_remove_key(struct PgpKeyInfo **klist, struct PgpKeyInfo *key);

struct PgpKeyInfo *pgp_keyinfo_new(void);

#endif /* MUTT_NCRYPT_PGPLIB_H */
