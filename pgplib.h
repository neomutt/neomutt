/*
 * Copyright (C) 1996,1997 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@guug.de>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */

#ifdef HAVE_PGP

#define PGPENCRYPT  (1 << 0)
#define PGPSIGN     (1 << 1)
#define PGPKEY      (1 << 2)
#define PGPGOODSIGN (1 << 3)

#define KEYFLAG_CANSIGN 		(1 <<  0)
#define KEYFLAG_CANENCRYPT 		(1 <<  1)
#define KEYFLAG_SECRET			(1 <<  7)
#define KEYFLAG_EXPIRED 		(1 <<  8)
#define KEYFLAG_REVOKED 		(1 <<  9)
#define KEYFLAG_DISABLED 		(1 << 10)
#define KEYFLAG_SUBKEY 			(1 << 11)
#define KEYFLAG_CRITICAL 		(1 << 12)
#define KEYFLAG_PREFER_ENCRYPTION 	(1 << 13)
#define KEYFLAG_PREFER_SIGNING 		(1 << 14)

#define KEYFLAG_CANTUSE (KEYFLAG_DISABLED|KEYFLAG_REVOKED|KEYFLAG_EXPIRED)
#define KEYFLAG_RESTRICTIONS (KEYFLAG_CANTUSE|KEYFLAG_CRITICAL)

#define KEYFLAG_ABILITIES (KEYFLAG_CANSIGN|KEYFLAG_CANENCRYPT|KEYFLAG_PREFER_ENCRYPTION|KEYFLAG_PREFER_SIGNING)

typedef struct pgp_signature
{
  struct pgp_signature *next;
  unsigned char sigtype;
  unsigned long sid1;
  unsigned long sid2;
}
pgp_sig_t;

typedef struct pgp_keyinfo
{
  char *keyid;
  struct pgp_uid *address;
  int flags;
  short keylen;
  time_t gen_time;
  int numalg;
  const char *algorithm;
  struct pgp_keyinfo *parent;
  struct pgp_signature *sigs;
  struct pgp_keyinfo *next;
}
pgp_key_t;

typedef struct pgp_uid
{
  char *addr;
  short trust;
  int flags;
  struct pgp_keyinfo *parent;
  struct pgp_uid *next;
  struct pgp_signature *sigs;
}
pgp_uid_t;

enum pgp_version
{
  PGP_V2,
  PGP_V3,
  PGP_GPG,
  PGP_UNKNOWN
};

enum pgp_ring
{
  PGP_PUBRING,
  PGP_SECRING
};

typedef enum pgp_ring pgp_ring_t;

/* prototypes */

const char *pgp_pkalgbytype (unsigned char);

pgp_key_t *pgp_remove_key (pgp_key_t **, pgp_key_t *);
pgp_uid_t *pgp_copy_uids (pgp_uid_t *, pgp_key_t *);

short pgp_canencrypt (unsigned char);
short pgp_cansign (unsigned char);
short pgp_get_abilities (unsigned char);

void pgp_free_key (pgp_key_t **kpp);

#define pgp_new_keyinfo() safe_calloc (sizeof (pgp_key_t), 1)

#endif /* HAVE_PGP */
