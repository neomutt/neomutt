/*
 * Copyright (C) 1996,1997 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1999 Thoms Roessler <roessler@guug.de>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef _PGPPATH

#define PGPENCRYPT 1
#define PGPSIGN    2
#define PGPKEY     4

#define KEYFLAG_CANSIGN 		(1 <<  0)
#define KEYFLAG_CANENCRYPT 		(1 <<  1)
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

typedef struct pgp_keyinfo
{
  char *keyid;
  struct pgp_uid *address;
  int flags;
  short keylen;
  time_t gen_time;
  const char *algorithm;
  struct pgp_keyinfo *parent;
  struct pgp_keyinfo *next;
}
pgp_key_t;

typedef struct pgp_uid
{
  char *addr;
  short trust;
  struct pgp_keyinfo *parent;
  struct pgp_uid *next;
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

enum pgp_ops
{
  PGP_DECODE,			/* application/pgp */
  PGP_VERIFY,			/* PGP/MIME, signed */
  PGP_DECRYPT,			/* PGP/MIME, encrypted */
  PGP_SIGN,			/* sign data */
  PGP_ENCRYPT,			/* encrypt data */
  PGP_IMPORT,			/* extract keys from messages */
  PGP_VERIFY_KEY,		/* verify key when selecting */
  PGP_EXPORT,			/* extract keys from key ring */
  PGP_LAST_OP
};

struct pgp_vinfo
{

  /* data */

  enum pgp_version v;
  char *name;
  char **binary;
  char **pubring;
  char **secring;
  char **language;

  /* functions */

  pgp_key_t *(*get_candidates) (struct pgp_vinfo *, pgp_ring_t, LIST *);
  
  pid_t (*invoke_decode) (struct pgp_vinfo *, FILE **, FILE **, FILE **,
			  int, int, int,
			  const char *, int);
  
  pid_t (*invoke_verify) (struct pgp_vinfo *, FILE **, FILE **, FILE **,
			  int, int, int,
			  const char *, const char *);
  
  pid_t (*invoke_decrypt) (struct pgp_vinfo *, FILE **, FILE **, FILE **,
			   int, int, int,
			   const char *);
  
  pid_t (*invoke_sign) (struct pgp_vinfo *, FILE **, FILE **, FILE **,
			int, int, int,
			const char *);
  
  pid_t (*invoke_encrypt) (struct pgp_vinfo *, FILE **, FILE **, FILE **,
			   int, int, int,
			   const char *, const char *, int);
  
  void (*invoke_import) (struct pgp_vinfo *, const char *);
  
  pid_t (*invoke_export) (struct pgp_vinfo *, FILE **, FILE **, FILE **,
			  int, int, int,
			  const char *);
  
  pid_t (*invoke_verify_key) (struct pgp_vinfo *, FILE **, FILE **, FILE **,
			      int, int, int,
			      const char *);
  
};


WHERE char *PgpV2;
WHERE char *PgpV2Language;
WHERE char *PgpV2Pubring;
WHERE char *PgpV2Secring;

WHERE char *PgpV3;
WHERE char *PgpV3Language;
WHERE char *PgpV3Pubring;
WHERE char *PgpV3Secring;

WHERE char *PgpGpg;
#if 0
WHERE char *PgpGpgLanguage;
WHERE char *PgpGpgPubring;
WHERE char *PgpGpgSecring;
#else
WHERE char *PgpGpgDummy;
#endif

WHERE char *PgpSendVersion;
WHERE char *PgpReceiveVersion;
WHERE char *PgpKeyVersion;
WHERE char *PgpDefaultVersion;

WHERE char *PgpSignAs;
WHERE char *PgpSignMicalg;

WHERE short PgpTimeout;

WHERE char *PgpEntryFormat;

BODY *pgp_decrypt_part (BODY *, STATE *, FILE *);
BODY *pgp_make_key_attachment (char *);

char *_pgp_keyid (pgp_key_t *);
char *pgp_keyid (pgp_key_t *);

const char *pgp_pkalg_to_mic (const char *);
const char *pgp_pkalgbytype (unsigned char);

int mutt_check_pgp (HEADER * h);
int mutt_is_application_pgp (BODY *);
int mutt_is_multipart_encrypted (BODY *);
int mutt_is_multipart_signed (BODY *);
int mutt_parse_pgp_hdr (char *, int);
int pgp_decrypt_mime (FILE *, FILE **, BODY *, BODY **);
int pgp_get_keys (HEADER *, char **);
int pgp_protect (HEADER *, char *);
int pgp_query (BODY *);
int pgp_string_matches_hint (const char *s, LIST * hints);
int pgp_valid_passphrase (void);

pgp_key_t *gpg_get_candidates (struct pgp_vinfo *, pgp_ring_t, LIST *);
pgp_key_t *pgp_ask_for_key (struct pgp_vinfo *, char *, char *, short, pgp_ring_t);
pgp_key_t *pgp_get_candidates (struct pgp_vinfo *, pgp_ring_t, LIST *);
pgp_key_t *pgp_getkeybyaddr (struct pgp_vinfo *pgp, ADDRESS *, short, pgp_ring_t);
pgp_key_t *pgp_getkeybystr (struct pgp_vinfo *pgp, char *, short, pgp_ring_t);
pgp_key_t *pgp_remove_key (pgp_key_t **, pgp_key_t *);

pgp_uid_t *pgp_copy_uids (pgp_uid_t *, pgp_key_t *);

short pgp_canencrypt (unsigned char);
short pgp_cansign (unsigned char);
short pgp_get_abilities (unsigned char);

struct pgp_vinfo *pgp_get_vinfo (enum pgp_ops);

void mutt_forget_passphrase (void);
void pgp_application_pgp_handler (BODY *, STATE *);
void pgp_encrypted_handler (BODY *, STATE *);
void pgp_extract_keys_from_attachment_list (FILE * fp, int tag, BODY * top);
void pgp_extract_keys_from_messages (HEADER * hdr);
void pgp_free_key (pgp_key_t **kpp);
void pgp_signed_handler (BODY *, STATE *);
void pgp_void_passphrase (void);

#define pgp_secring(a) pgp_getring(a, 0)
#define pgp_pubring(a) pgp_getring(a, 1)

/* PGP V2 prototypes */


pid_t pgp_v2_invoke_decode (struct pgp_vinfo *,
			    FILE **, FILE **, FILE **,
			    int, int, int,
			    const char *, int);

pid_t pgp_v2_invoke_verify (struct pgp_vinfo *,
			    FILE **, FILE **, FILE **,
			    int, int, int,
			    const char *, const char *);


pid_t pgp_v2_invoke_decrypt (struct pgp_vinfo *,
			     FILE **, FILE **, FILE **,
			     int, int, int,
			     const char *);

pid_t pgp_v2_invoke_sign (struct pgp_vinfo *,
			  FILE **, FILE **, FILE **,
			  int, int, int,
			  const char *);

pid_t pgp_v2_invoke_encrypt (struct pgp_vinfo *,
			     FILE **, FILE **, FILE **,
			     int, int, int,
			     const char *, const char *, int);

void pgp_v2_invoke_import (struct pgp_vinfo *, const char *);

pid_t pgp_v2_invoke_export (struct pgp_vinfo *,
			    FILE **, FILE **, FILE **,
			    int, int, int,
			    const char *);

pid_t pgp_v2_invoke_verify_key (struct pgp_vinfo *,
				FILE **, FILE **, FILE **,
				int, int, int,
				const char *);

/* PGP V3 prototypes */

pid_t pgp_v3_invoke_decode (struct pgp_vinfo *,
			    FILE **, FILE **, FILE **,
			    int, int, int,
			    const char *, int);

pid_t pgp_v3_invoke_verify (struct pgp_vinfo *,
			    FILE **, FILE **, FILE **,
			    int, int, int,
			    const char *, const char *);


pid_t pgp_v3_invoke_decrypt (struct pgp_vinfo *,
			     FILE **, FILE **, FILE **,
			     int, int, int,
			     const char *);

pid_t pgp_v3_invoke_sign (struct pgp_vinfo *,
			  FILE **, FILE **, FILE **,
			  int, int, int,
			  const char *);

pid_t pgp_v3_invoke_encrypt (struct pgp_vinfo *,
			     FILE **, FILE **, FILE **,
			     int, int, int,
			     const char *, const char *, int);

void pgp_v3_invoke_import (struct pgp_vinfo *, const char *);

pid_t pgp_v3_invoke_export (struct pgp_vinfo *,
			    FILE **, FILE **, FILE **,
			    int, int, int,
			    const char *);

pid_t pgp_v3_invoke_verify_key (struct pgp_vinfo *,
				FILE **, FILE **, FILE **,
				int, int, int,
				const char *);

/* GNU Privacy Guard Prototypes */

pid_t pgp_gpg_invoke_decode (struct pgp_vinfo *,
			     FILE **, FILE **, FILE **,
			     int, int, int,
			     const char *, int);

pid_t pgp_gpg_invoke_verify (struct pgp_vinfo *,
			     FILE **, FILE **, FILE **,
			     int, int, int,
			     const char *, const char *);


pid_t pgp_gpg_invoke_decrypt (struct pgp_vinfo *,
			      FILE **, FILE **, FILE **,
			      int, int, int,
			      const char *);

pid_t pgp_gpg_invoke_sign (struct pgp_vinfo *,
			   FILE **, FILE **, FILE **,
			   int, int, int,
			   const char *);

pid_t pgp_gpg_invoke_encrypt (struct pgp_vinfo *,
			      FILE **, FILE **, FILE **,
			      int, int, int,
			      const char *, const char *, int);

void pgp_gpg_invoke_import (struct pgp_vinfo *, const char *);

pid_t pgp_gpg_invoke_export (struct pgp_vinfo *,
			     FILE **, FILE **, FILE **,
			     int, int, int,
			     const char *);

pid_t pgp_gpg_invoke_verify_key (struct pgp_vinfo *,
				 FILE **, FILE **, FILE **,
				 int, int, int,
				 const char *);




#if 0

/* use these as templates for your own prototypes */


pid_t pgp_VERSION_invoke_decode (struct pgp_vinfo *,
				 FILE **, FILE **, FILE **,
				 int, int, int,
				 const char *, int);

pid_t pgp_VERSION_invoke_verify (struct pgp_vinfo *,
				 FILE **, FILE **, FILE **,
				 int, int, int,
				 const char *, const char *);


pid_t pgp_VERSION_invoke_decrypt (struct pgp_vinfo *,
				  FILE **, FILE **, FILE **,
				  int, int, int,
				  const char *);

pid_t pgp_VERSION_invoke_sign (struct pgp_vinfo *,
			       FILE **, FILE **, FILE **,
			       int, int, int,
			       const char *);

pid_t pgp_VERSION_invoke_encrypt (struct pgp_vinfo *,
				  FILE **, FILE **, FILE **,
				  int, int, int,
				  const char *, const char *, int);

void pgp_VERSION_invoke_import (struct pgp_vinfo *, const char *);

pid_t pgp_VERSION_invoke_export (struct pgp_vinfo *,
				 FILE **, FILE **, FILE **,
				 int, int, int,
				 const char *);

pid_t pgp_VERSION_invoke_verify_key (struct pgp_vinfo *,
				     FILE **, FILE **, FILE **,
				     int, int, int,
				     const char *);

#endif

#endif /* _PGPPATH */
