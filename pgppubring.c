/*
 * Copyright (C) 1997-1999 Thomas Roessler <roessler@guug.de>
 * 
 *     This program is free software; you can redistribute it
 *     and/or modify it under the terms of the GNU General Public
 *     License as published by the Free Software Foundation; either
 *     version 2 of the License, or (at your option) any later
 *     version.
 * 
 *     This program is distributed in the hope that it will be
 *     useful, but WITHOUT ANY WARRANTY; without even the implied
 *     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *     PURPOSE.  See the GNU General Public License for more
 *     details.
 * 
 *     You should have received a copy of the GNU General Public
 *     License along with this program; if not, write to the Free
 *     Software Foundation, Inc., 675 Mass Ave, Cambridge, MA
 *     02139, USA.
 */

/*
 * This is a "simple" PGP key ring dumper.
 * 
 * The output format is supposed to be compatible to the one GnuPG
 * emits and Mutt expects.
 * 
 * Note that the code of this program could be considerably less
 * complex, but most of it was taken from mutt's second generation
 * key ring parser.
 * 
 * You can actually use this to put together some fairly general
 * PGP key management applications.
 *
 */


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#ifdef HAVE_GETOPT_H
# include <getopt.h>
#else
# ifndef HAVE_GETOPT_DECL
extern char *optarg;
extern int optind;
# endif
#endif

#include "sha.h"
#include "lib.h"
#include "pgplib.h"

#ifdef HAVE_FGETPOS
#define FGETPOS(fp,pos) fgetpos((fp),&(pos))
#define FSETPOS(fp,pos) fsetpos((fp),&(pos))
#else
#define FGETPOS(fp,pos) pos=ftell((fp));
#define FSETPOS(fp,pos) fseek((fp),(pos),SEEK_SET)
#endif

#define CHUNKSIZE 1024

static unsigned char *pbuf = NULL;
static size_t plen = 0;

static void pgpring_find_candidates (char *ringfile, const char *hints[], int nhints);
static void pgpring_dump_keyblock (pgp_key_t *p);

int main (int argc, char * const argv[])
{
  int c;
  
  short version = 2;
  short secring = 0;
  
  const char *_kring = NULL;
  char *env_pgppath, *env_home;

  char pgppath[_POSIX_PATH_MAX];
  char kring[_POSIX_PATH_MAX];

  while ((c = getopt (argc, argv, "25sk:")) != EOF)
  {
    switch (c)
    {
      case 'k':
      {
	_kring = optarg;
	break;
      }
      
      case '2': case '5':
      {
	version = c - '0';
	break;
      }
      
      case 's':
      {
	secring = 1;
	break;
      }
    
      default:
      {
	fprintf (stderr, "usage: %s [-k <key ring> | [-2 | -5] [ -s]] [hints]\n",
		 argv[0]);
	exit (1);
      }
    }
  }

  if (_kring)
    strfcpy (kring, _kring, sizeof (kring));
  else
  {
    if ((env_pgppath = getenv ("PGPPATH")))
      strfcpy (pgppath, env_pgppath, sizeof (pgppath));
    else if ((env_home = getenv ("HOME")))
      snprintf (pgppath, sizeof (pgppath), "%s/.pgp", env_home);
    else
    {
      fprintf (stderr, "%s: Can't determine your PGPPATH.\n", argv[0]);
      exit (1);
    }
    
    if (secring)
      snprintf (kring, sizeof (kring), "%s/secring.%s", pgppath, version == 2 ? "pgp" : "skr");
    else
      snprintf (kring, sizeof (kring), "%s/pubring.%s", pgppath, version == 2 ? "pgp" : "pkr");
  }
  
  pgpring_find_candidates (kring, argv + optind, argc - optind);
    
  return 0;
}


/* The actual key ring parser */

enum packet_tags
{
  PT_RES0 = 0,			/* reserved */
  PT_ESK,			/* Encrypted Session Key */
  PT_SIG,			/* Signature Packet */
  PT_CESK,			/* Conventionally Encrypted Session Key Packet */
  PT_OPS,			/* One-Pass Signature Packet */
  PT_SECKEY,			/* Secret Key Packet */
  PT_PUBKEY,			/* Public Key Packet */
  PT_SUBSECKEY,			/* Secret Subkey Packet */
  PT_COMPRESSED,		/* Compressed Data Packet */
  PT_SKE,			/* Symmetrically Encrypted Data Packet */
  PT_MARKER,			/* Marker Packet */
  PT_LITERAL,			/* Literal Data Packet */
  PT_TRUST,			/* Trust Packet */
  PT_NAME,			/* Name Packet */
  PT_SUBKEY,			/* Subkey Packet */
  PT_RES15,			/* Reserved */
  PT_COMMENT			/* Comment Packet */
};

/* 
 * These aren't displayed at all currently, but they may come quite
 * handy for debugging strings.
 * 
 * No need to translate them.
 *
 */

const char *pgp_packet_name[] =
{
  "reserved",
  "Encrypted Session Key",
  "Signature Packet",
  "Conventionally Encrypted Session Key Packet",
  "One-Pass Signature Packet",
  "Secret Key Packet",
  "Public Key Packet",
  "Secret Subkey Packet",
  "Compressed Data Packet",
  "Symmetrically Encrypted Data Packet",
  "Marker Packet",
  "Literal Data Packet",
  "Trust Packet",
  "Name Packet",
  "Subkey Packet",
  "Reserved",
  "Comment Packet"
};

static int read_material (size_t material, size_t * used, FILE * fp)
{
  if (*used + material >= plen)
  {
    unsigned char *p;
    size_t nplen;

    nplen = *used + material + CHUNKSIZE;

    if (!(p = realloc (pbuf, nplen)))
    {
      perror ("realloc");
      return -1;
    }
    plen = nplen;
    pbuf = p;
  }

  if (fread (pbuf + *used, 1, material, fp) < material)
  {
    perror ("fread");
    return -1;
  }

  *used += material;
  return 0;
}

static unsigned char *pgp_read_packet (FILE * fp, size_t * len)
{
  size_t used = 0;
  long startpos;
  unsigned char ctb;
  unsigned char b;
  size_t material;

  startpos = ftell (fp);

  if (!plen)
  {
    plen = CHUNKSIZE;
    pbuf = safe_malloc (plen);
  }

  if (fread (&ctb, 1, 1, fp) < 1)
  {
    if (!feof (fp))
      perror ("fread");
    goto bail;
  }

  if (!(ctb & 0x80))
  {
    goto bail;
  }

  if (ctb & 0x40)		/* handle PGP 5.0 packets. */
  {
    int partial = 0;
    pbuf[0] = ctb;
    used++;

    do
    {
      if (fread (&b, 1, 1, fp) < 1)
      {
	perror ("fread");
	goto bail;
      }

      if (b < 192)
      {
	material = b;
	partial = 0;
	material -= 1;
      }
      else if (192 <= b && b <= 223)
      {
	material = (b - 192) * 256;
	if (fread (&b, 1, 1, fp) < 1)
	{
	  perror ("fread");
	  goto bail;
	}
	material += b + 192;
	partial = 0;
	material -= 2;
      }
      else if (b < 255)
      {
	material = 1 << (b & 0x1f);
	partial = 1;
	material -= 1;
      }
      else
	/* b == 255 */
      {
	unsigned char buf[4];
	if (fread (buf, 4, 1, fp) < 1)
	{
	  perror ("fread");
	  goto bail;
	}
	/*assert( sizeof(material) >= 4 ); */
	material = buf[0] << 24;
	material |= buf[1] << 16;
	material |= buf[2] << 8;
	material |= buf[3];
	partial = 0;
	material -= 5;
      }

      if (read_material (material, &used, fp) == -1)
	goto bail;

    }
    while (partial);
  }
  else
    /* Old-Style PGP */
  {
    int bytes = 0;
    pbuf[0] = 0x80 | ((ctb >> 2) & 0x0f);
    used++;

    switch (ctb & 0x03)
    {
      case 0:
      {
	if (fread (&b, 1, 1, fp) < 1)
	{
	  perror ("fread");
	  goto bail;
	}

	material = b;
	break;
      }

      case 1:
      bytes = 2;

      case 2:
      {
	int i;

	if (!bytes)
	  bytes = 4;

	material = 0;

	for (i = 0; i < bytes; i++)
	{
	  if (fread (&b, 1, 1, fp) < 1)
	  {
	    perror ("fread");
	    goto bail;
	  }

	  material = (material << 8) + b;
	}
	break;
      }

      default:
      goto bail;
    }

    if (read_material (material, &used, fp) == -1)
      goto bail;
  }

  if (len)
    *len = used;

  return pbuf;

bail:

  fseek (fp, startpos, SEEK_SET);
  return NULL;
}

static pgp_key_t *pgp_parse_pgp2_key (unsigned char *buff, size_t l)
{
  pgp_key_t *p;
  unsigned char alg;
  size_t expl;
  unsigned long id;
  time_t gen_time = 0;
  unsigned short exp_days = 0;
  size_t j;
  int i, k;
  unsigned char scratch[LONG_STRING];

  if (l < 12)
    return NULL;

  p = pgp_new_keyinfo();

  for (i = 0, j = 2; i < 4; i++)
    gen_time = (gen_time << 8) + buff[j++];

  p->gen_time = gen_time;

  for (i = 0; i < 2; i++)
    exp_days = (exp_days << 8) + buff[j++];

  if (exp_days && time (NULL) > gen_time + exp_days * 24 * 3600)
    p->flags |= KEYFLAG_EXPIRED;

  alg = buff[j++];

  p->numalg = alg;
  p->algorithm = pgp_pkalgbytype (alg);
  p->flags |= pgp_get_abilities (alg);

  expl = 0;
  for (i = 0; i < 2; i++)
    expl = (expl << 8) + buff[j++];

  p->keylen = expl;

  expl = (expl + 7) / 8;
  if (expl < 4)
    goto bailout;


  j += expl - 8;

  for (k = 0; k < 2; k++)
  {
    for (id = 0, i = 0; i < 4; i++)
      id = (id << 8) + buff[j++];

    snprintf ((char *) scratch + k * 8, sizeof (scratch) - k * 8,
	      "%08lX", id);
  }

  p->keyid = safe_strdup ((char *) scratch);

  return p;

bailout:

  safe_free ((void **) &p);
  return NULL;
}

static void pgp_make_pgp3_fingerprint (unsigned char *buff, size_t l,
				       unsigned char *digest)
{
  unsigned char dummy;
  SHA_CTX context;

  SHA1_Init (&context);

  dummy = buff[0] & 0x3f;

  if (dummy == PT_SUBSECKEY || dummy == PT_SUBKEY || dummy == PT_SECKEY)
    dummy = PT_PUBKEY;

  dummy = (dummy << 2) | 0x81;
  SHA1_Update (&context, &dummy, 1);
  dummy = ((l - 1) >> 8) & 0xff;
  SHA1_Update (&context, &dummy, 1);
  dummy = (l - 1) & 0xff;
  SHA1_Update (&context, &dummy, 1);
  SHA1_Update (&context, buff + 1, l - 1);
  SHA1_Final (digest, &context);

}

static void skip_bignum (unsigned char *buff, size_t l, size_t j,
			 size_t * toff, size_t n)
{
  size_t len;

  do
  {
    len = (buff[j] << 8) + buff[j + 1];
    j += (len + 7) / 8 + 2;
  }
  while (j <= l && --n > 0);

  if (toff)
    *toff = j;
}


static pgp_key_t *pgp_parse_pgp3_key (unsigned char *buff, size_t l)
{
  pgp_key_t *p;
  unsigned char alg;
  unsigned char digest[SHA_DIGEST_LENGTH];
  unsigned char scratch[LONG_STRING];
  time_t gen_time = 0;
  unsigned long id;
  int i, k;
  short len;
  size_t j;

  p = pgp_new_keyinfo ();
  j = 2;

  for (i = 0; i < 4; i++)
    gen_time = (gen_time << 8) + buff[j++];

  p->gen_time = gen_time;

  alg = buff[j++];

  p->numalg = alg;
  p->algorithm = pgp_pkalgbytype (alg);
  p->flags |= pgp_get_abilities (alg);

  if (alg == 17)
    skip_bignum (buff, l, j, &j, 3);
  else if (alg == 16 || alg == 20)
    skip_bignum (buff, l, j, &j, 2);

  len = (buff[j] << 8) + buff[j + 1];
  p->keylen = len;

  if (alg >= 1 && alg <= 3)
    skip_bignum (buff, l, j, &j, 2);
  else if (alg == 17 || alg == 16)
    skip_bignum (buff, l, j, &j, 1);

  pgp_make_pgp3_fingerprint (buff, j, digest);

  for (k = 0; k < 2; k++)
  {
    for (id = 0, i = SHA_DIGEST_LENGTH - 8 + k * 4;
	 i < SHA_DIGEST_LENGTH + (k - 1) * 4; i++)
      id = (id << 8) + digest[i];

    snprintf ((char *) scratch + k * 8, sizeof (scratch) - k * 8, "%08lX", id);
  }

  p->keyid = safe_strdup ((char *) scratch);

  return p;
}

static pgp_key_t *pgp_parse_keyinfo (unsigned char *buff, size_t l)
{
  if (!buff || l < 2)
    return NULL;

  switch (buff[1])
  {
  case 2:
  case 3:
    return pgp_parse_pgp2_key (buff, l);
  case 4:
    return pgp_parse_pgp3_key (buff, l);
  default:
    return NULL;
  }
}

static int pgp_parse_pgp2_sig (unsigned char *buff, size_t l, pgp_key_t * p)
{
  unsigned char sigtype;
  time_t sig_gen_time;
  unsigned long signerid;
  size_t j;
  int i;

  if (l < 22)
    return -1;

  j = 3;
  sigtype = buff[j++];

  sig_gen_time = 0;
  for (i = 0; i < 4; i++)
    sig_gen_time = (sig_gen_time << 8) + buff[j++];

  j += 4;
  signerid = 0;
  for (i = 0; i < 4; i++)
    signerid = (signerid << 8) + buff[j++];

  if (sigtype == 0x20 || sigtype == 0x28)
    p->flags |= KEYFLAG_REVOKED;

  return 0;
}

static int pgp_parse_pgp3_sig (unsigned char *buff, size_t l, pgp_key_t * p)
{
  unsigned char sigtype;
  unsigned char pkalg;
  unsigned char hashalg;
  unsigned char skt;
  time_t sig_gen_time = -1;
  long validity = -1;
  long key_validity = -1;
  long signerid = 0;
  size_t ml;
  size_t j;
  int i;
  short ii;
  short have_critical_spks = 0;

  if (l < 7)
    return -1;

  j = 2;

  sigtype = buff[j++];
  pkalg = buff[j++];
  hashalg = buff[j++];

  for (ii = 0; ii < 2; ii++)
  {
    size_t skl;
    size_t nextone;

    ml = (buff[j] << 8) + buff[j + 1];
    j += 2;

    if (j + ml > l)
      break;

    nextone = j;
    while (ml)
    {
      j = nextone;
      skl = buff[j++];
      if (!--ml)
	break;

      if (skl >= 192)
      {
	skl = (skl - 192) * 256 + buff[j++] + 192;
	if (!--ml)
	  break;
      }

      if ((int) ml - (int) skl < 0)
	break;
      ml -= skl;

      nextone = j + skl;
      skt = buff[j++];

      switch (skt & 0x7f)
      {
	case 2:			/* creation time */
	{
	  if (skl < 4)
	    break;
	  sig_gen_time = 0;
	  for (i = 0; i < 4; i++)
	    sig_gen_time = (sig_gen_time << 8) + buff[j++];

	  break;
	}
	case 3:			/* expiration time */
	{
	  if (skl < 4)
	    break;
	  validity = 0;
	  for (i = 0; i < 4; i++)
	    validity = (validity << 8) + buff[j++];
	  break;
	}
	case 9:			/* key expiration time */
	{
	  if (skl < 4)
	    break;
	  key_validity = 0;
	  for (i = 0; i < 4; i++)
	    key_validity = (key_validity << 8) + buff[j++];
	  break;
	}
	case 16:			/* issuer key ID */
	{
	  if (skl < 8)
	    break;
	  j += 4;
	  signerid = 0;
	  for (i = 0; i < 4; i++)
	    signerid = (signerid << 8) + buff[j++];
	  break;
	}
	case 10:			/* CMR key */
	break;
	case 4:				/* exportable */
	case 5:				/* trust */
	case 6:				/* regexp */
	case 7:				/* revocable */
	case 11:			/* Pref. symm. alg. */
	case 12:			/* revocation key */
	case 20:			/* notation data */
	case 21:			/* pref. hash */
	case 22:			/* pref. comp.alg. */
	case 23:			/* key server prefs. */
	case 24:			/* pref. key server */
	default:
	{
	  if (skt & 0x80)
	    have_critical_spks = 1;
	}
      }
    }
    j = nextone;
  }

  if (sigtype == 0x20 || sigtype == 0x28)
    p->flags |= KEYFLAG_REVOKED;
  if (key_validity != -1 && time (NULL) > p->gen_time + key_validity)
    p->flags |= KEYFLAG_EXPIRED;
  if (have_critical_spks)
    p->flags |= KEYFLAG_CRITICAL;

  return 0;

}


static int pgp_parse_sig (unsigned char *buff, size_t l, pgp_key_t * p)
{
  if (!buff || l < 2 || !p)
    return -1;

  switch (buff[1])
  {
  case 2:
  case 3:
    return pgp_parse_pgp2_sig (buff, l, p);
  case 4:
    return pgp_parse_pgp3_sig (buff, l, p);
  default:
    return -1;
  }
}

/* parse one key block, including all subkeys. */

static pgp_key_t *pgp_parse_keyblock (FILE * fp)
{
  unsigned char *buff;
  unsigned char pt = 0;
  unsigned char last_pt;
  size_t l;
  short err = 0;

#ifdef HAVE_FGETPOS
  fpos_t pos;
#else
  long pos;
#endif

  pgp_key_t *root = NULL;
  pgp_key_t **last = &root;
  pgp_key_t *p = NULL;
  pgp_uid_t *uid = NULL;
  pgp_uid_t **addr = NULL;

  FGETPOS(fp,pos);
  
  while (!err && (buff = pgp_read_packet (fp, &l)) != NULL)
  {
    last_pt = pt;
    pt = buff[0] & 0x3f;

    /* check if we have read the complete key block. */
    
    if ((pt == PT_SECKEY || pt == PT_PUBKEY) && root)
    {
      FSETPOS(fp, pos);
      return root;
    }
    
    switch (pt)
    {
      case PT_SECKEY:
      case PT_PUBKEY:
      case PT_SUBKEY:
      case PT_SUBSECKEY:
      {
	if (!(*last = p = pgp_parse_keyinfo (buff, l)))
	{
	  err = 1;
	  break;
	}

	last = &p->next;
	addr = &p->address;
	
	if (pt == PT_SUBKEY || pt == PT_SUBSECKEY)
	{
	  p->flags |= KEYFLAG_SUBKEY;
	  if (p != root)
	  {
	    p->parent  = root;
	    p->address = pgp_copy_uids (root->address, p);
	    while (*addr) addr = &(*addr)->next;
	  }
	}
	
	if (pt == PT_SECKEY || pt == PT_SUBSECKEY)
	  p->flags |= KEYFLAG_SECRET;

	break;
      }

      case PT_SIG:
      {
	pgp_parse_sig (buff, l, p);
	break;
      }

      case PT_TRUST:
      {
	if (p && (last_pt == PT_SECKEY || last_pt == PT_PUBKEY ||
		  last_pt == PT_SUBKEY || last_pt == PT_SUBSECKEY))
	{
	  if (buff[1] & 0x20)
	  {
	    p->flags |= KEYFLAG_DISABLED;
	  }
	}
	else if (last_pt == PT_NAME && uid)
	{
	  uid->trust = buff[1];
	}
	break;
      }
      case PT_NAME:
      {
	char *chr;


	if (!addr)
	  break;

	chr = safe_malloc (l);
	memcpy (chr, buff + 1, l - 1);
	chr[l - 1] = '\0';


	*addr = uid = safe_calloc (1, sizeof (pgp_uid_t)); /* XXX */
	uid->addr = chr;
	uid->parent = p;
	uid->trust = 0;
	addr = &uid->next;

	/* the following tags are generated by
	 * pgp 2.6.3in.
	 */

	if (strstr (chr, "ENCR"))
	  p->flags |= KEYFLAG_PREFER_ENCRYPTION;
	if (strstr (chr, "SIGN"))
	  p->flags |= KEYFLAG_PREFER_SIGNING;

	break;
      }
    }

    FGETPOS(fp,pos);
  }

  if (err)
    pgp_free_key (&root);
  
  return root;  
}

static int pgpring_string_matches_hint (const char *s, const char *hints[], int nhints)
{
  int i;

  if (!hints || !nhints)
    return 1;

  for (i = 0; i < nhints; i++)
  {
    if (mutt_stristr (s, hints[i]) != NULL)
      return 1;
  }

  return 0;
}

/* 
 * Go through the key ring file and look for keys with
 * matching IDs.
 */

static void pgpring_find_candidates (char *ringfile, const char *hints[], int nhints)
{
  FILE *rfp;
#ifdef HAVE_FGETPOS
  fpos_t pos, keypos;
#else
  long pos, keypos;
#endif

  unsigned char *buff = NULL;
  unsigned char pt = 0;
  size_t l = 0;

  short err = 0;
  
  if ((rfp = fopen (ringfile, "r")) == NULL)
  {
    perror ("fopen");
    return;
  }

  FGETPOS(rfp,pos);
  FGETPOS(rfp,keypos);

  while (!err && (buff = pgp_read_packet (rfp, &l)) != NULL)
  {
    pt = buff[0] & 0x3f;
    
    if (l < 1)
      continue;
    
    if ((pt == PT_SECKEY) || (pt == PT_PUBKEY))
    {
      keypos = pos;
    }
    else if (pt == PT_NAME)
    {
      char *tmp = safe_malloc (l);

      memcpy (tmp, buff + 1, l - 1);
      tmp[l - 1] = '\0';

      /* mutt_decode_utf8_string (tmp, chs); */

      if (pgpring_string_matches_hint (tmp, hints, nhints))
      {
	pgp_key_t *p;

	FSETPOS(rfp, keypos);

	/* Not bailing out here would lead us into an endless loop. */

	if ((p = pgp_parse_keyblock (rfp)) == NULL)
	  err = 1;
	
	pgpring_dump_keyblock (p);
	pgp_free_key (&p);
      }

      safe_free ((void **) &tmp);
    }

    FGETPOS(rfp,pos);
  }

  fclose (rfp);

}

static void print_userid (const char *id)
{
  for (; id && *id; id++)
  {
    if (*id >= ' ' && *id <= 'z' && *id != ':')
      putchar (*id);
    else
      printf ("\\x%02x", *id);
  }
}

static char gnupg_trustletter (int t)
{
  switch (t)
  {
    case 1: return 'n';
    case 2: return 'm';
    case 3: return 'f';
  }
  return 'q';
}

static void pgpring_dump_keyblock (pgp_key_t *p)
{
  pgp_uid_t *uid;
  short first;
  struct tm *tp;
  time_t t;
  
  for (; p; p = p->next)
  {
    first = 1;

    if (p->flags & KEYFLAG_SECRET)
    {
      if (p->flags & KEYFLAG_SUBKEY)
	printf ("ssb:");
      else
	printf ("sec:");
    }
    else 
    {
      if (p->flags & KEYFLAG_SUBKEY)
	printf ("sub:");
      else
	printf ("pub:");
    }
    
    if (p->flags & KEYFLAG_REVOKED)
      putchar ('r');
    if (p->flags & KEYFLAG_EXPIRED)
      putchar ('e');
    
    for (uid = p->address; uid; uid = uid->next, first = 0)
    {
      if (!first)
      {
	printf ("uid:%c::::::::", gnupg_trustletter (uid->trust));
	print_userid (uid->addr);
	printf (":\n");
      }
      else
      {
	if (p->flags & KEYFLAG_SECRET)
	  putchar ('u');
	else
	  putchar (gnupg_trustletter (uid->trust));

	t = p->gen_time;
	tp = gmtime (&t);

	printf (":%d:%d:%s:%04d-%02d-%02d::::", p->keylen, p->numalg, p->keyid,
		1900 + tp->tm_year, tp->tm_mon + 1, tp->tm_mday);
	
	print_userid (uid->addr);
	printf (":\n");
      }
    }
  }
}
	
