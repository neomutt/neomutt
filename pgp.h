/*
 * Copyright (C) 1996,1997 Michael R. Elkins <me@cs.hmc.edu>
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

#define KEYFLAG_CANSIGN (1 << 0)
#define KEYFLAG_CANENCRYPT (1 << 1)
#define KEYFLAG_EXPIRED (1 << 8)
#define KEYFLAG_REVOKED (1 << 9)
#define KEYFLAG_DISABLED (1 << 10)
#define KEYFLAG_SUBKEY (1 << 11)
#define KEYFLAG_CRITICAL (1 << 12)
#define KEYFLAG_PREFER_ENCRYPTION (1 << 13)
#define KEYFLAG_PREFER_SIGNING (1 << 14)

typedef struct keyinfo
{
  char *keyid;
  LIST *address;
  short flags;
  short keylen;
  unsigned long gen_time;
  const char *algorithm;
  struct keyinfo *mainkey;
  struct keyinfo *next;
} KEYINFO;

typedef struct pgp_uid
{
  char *addr;
  short trust;
} PGPUID;

enum pgp_version 
{
  PGP_V2, 
  PGP_V3, 
  PGP_UNKNOWN
};

enum pgp_ops
{
  PGP_DECODE,		/* application/pgp */
  PGP_VERIFY,		/* PGP/MIME, signed */
  PGP_DECRYPT,		/* PGP/MIME, encrypted */
  PGP_SIGN,		/* sign data */
  PGP_ENCRYPT,		/* encrypt data */
  PGP_EXTRACT,		/* extract keys from messages */
  PGP_VERIFY_KEY,	/* verify key when selecting */
  PGP_EXTRACT_KEY,	/* extract keys from key ring */
  PGP_LAST_OP
};

struct pgp_vinfo
{
  enum pgp_version v;
  char *name;
  char **binary;
  char **pubring;
  char **secring;
  char **language;
};


WHERE char *PgpV2;
WHERE char *PgpV2Language;
WHERE char *PgpV2Pubring;
WHERE char *PgpV2Secring;

WHERE char *PgpV3;
WHERE char *PgpV3Language;
WHERE char *PgpV3Pubring;
WHERE char *PgpV3Secring;

WHERE char *PgpSendVersion;
WHERE char *PgpReceiveVersion;
WHERE char *PgpKeyVersion;
WHERE char *PgpDefaultVersion;

WHERE char *PgpSignAs;
WHERE char *PgpSignMicalg;

WHERE short PgpTimeout;



BODY *pgp_decrypt_part (BODY *, STATE *, FILE *);
BODY *pgp_make_key_attachment (char *);

const char *pgp_pkalg_to_mic(const char *);

char *pgp_ask_for_key (const char *, KEYINFO *, char *, char *, short, char **);

char *pgp_binary(enum pgp_ops);
char *pgp_getring (enum pgp_ops, int);
char *pgp_keyid(KEYINFO *);
char *_pgp_keyid(KEYINFO *);

enum pgp_version pgp_version(enum pgp_ops);
struct pgp_vinfo *pgp_get_vinfo(enum pgp_ops);

int mutt_check_pgp (HEADER *h);
int mutt_parse_pgp_hdr (char *, int);

int pgp_protect (HEADER *);
int pgp_query (BODY *);
int pgp_valid_passphrase (void);

KEYINFO *ki_getkeybyaddr (ADDRESS *, KEYINFO *, short);
KEYINFO *ki_getkeybystr (char *, KEYINFO *, short);
KEYINFO *pgp_read_keyring(const char *);

void application_pgp_handler (BODY *, STATE *);
void mutt_forget_passphrase (void);
void pgp_closedb (KEYINFO *);
void pgp_encrypted_handler (BODY *, STATE *);
void pgp_extract_keys_from_attachment_list (FILE *fp, int tag, BODY *top);
void pgp_extract_keys_from_messages(HEADER *hdr);
void pgp_signed_handler (BODY *, STATE *);
void pgp_void_passphrase (void);

#define pgp_secring(a) pgp_getring(a, 0)
#define pgp_pubring(a) pgp_getring(a, 1)


pid_t pgp_invoke_decode(FILE **, FILE **, FILE **, int, int, int, const char *, int);
pid_t pgp_invoke_verify(FILE **, FILE **, FILE **, int, int, int, const char *, const char *);
pid_t pgp_invoke_decrypt(FILE **, FILE **, FILE **, int, int, int, const char *);
pid_t pgp_invoke_sign(FILE **, FILE **, FILE **, int, int, int, const char *);
pid_t pgp_invoke_encrypt(FILE **, FILE **, FILE **, int, int, int, const char *, const char *, int);
void  pgp_invoke_extract(const char *);
pid_t pgp_invoke_verify_key(FILE **, FILE **, FILE **, int, int, int, const char *);
pid_t pgp_invoke_extract_key(FILE **, FILE **, FILE **, int, int, int, const char *);


#endif /* _PGPPATH */
