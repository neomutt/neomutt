/**
 * Copyright (C) 1996-1997 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2002 Thomas Roessler <roessler@does-not-exist.org>
 *
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

#ifndef _MUTT_PGPLIB_H
#define _MUTT_PGPLIB_H 1

#ifdef CRYPT_BACKEND_CLASSIC_PGP

#include "mutt_crypt.h"


struct PgpSignature
{
  struct PgpSignature *next;
  unsigned char sigtype;
  unsigned long sid1;
  unsigned long sid2;
};

struct PgpKeyInfo
{
  char *keyid;
  char *fingerprint;
  struct PgpUid *address;
  int flags;
  short keylen;
  time_t gen_time;
  int numalg;
  const char *algorithm;
  struct PgpKeyInfo *parent;
  struct PgpSignature *sigs;
  struct PgpKeyInfo *next;
};

struct PgpUid
{
  char *addr;
  short trust;
  int flags;
  struct PgpKeyInfo *parent;
  struct PgpUid *next;
  struct PgpSignature *sigs;
};

enum pgp_version
{
  PGP_V2,
  PGP_V3,
  PGP_GPG,
  PGP_UNKNOWN
};

/* prototypes */

const char *pgp_pkalgbytype(unsigned char type);

struct PgpKeyInfo *pgp_remove_key(struct PgpKeyInfo **klist, struct PgpKeyInfo *key);
struct PgpUid *pgp_copy_uids(struct PgpUid *up, struct PgpKeyInfo *parent);

bool pgp_canencrypt(unsigned char type);
bool pgp_cansign(unsigned char type);
short pgp_get_abilities(unsigned char type);

void pgp_free_key(struct PgpKeyInfo **kpp);

static inline struct PgpKeyInfo *pgp_new_keyinfo(void)
{
  return safe_calloc(1, sizeof(struct PgpKeyInfo));
}

#endif /* CRYPT_BACKEND_CLASSIC_PGP */

#endif /* _MUTT_PGPLIB_H */
