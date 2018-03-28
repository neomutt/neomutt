/**
 * @file
 * Standalone tool to dump the contents of a PGP key ring
 *
 * @authors
 * Copyright (C) 1997-2003 Thomas Roessler <roessler@does-not-exist.org>
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

/*
 * This is a "simple" PGP key ring dumper.
 *
 * The output format is supposed to be compatible to the one GnuPG
 * emits and NeoMutt expects.
 *
 * Note that the code of this program could be considerably less
 * complex, but most of it was taken from neomutt's second generation
 * key ring parser.
 *
 * You can actually use this to put together some fairly general
 * PGP key management applications.
 */

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "ncrypt/ncrypt.h"
#include "ncrypt/pgplib.h"
#include "ncrypt/pgppacket.h"

extern char *optarg;
extern int optind;

#define MD5_DIGEST_LENGTH 16

static short dump_signatures = 0;
static short dump_fingerprints = 0;

static char gnupg_trustletter(int t)
{
  switch (t)
  {
    case 1:
      return 'n';
    case 2:
      return 'm';
    case 3:
      return 'f';
  }
  return 'q';
}

static void print_userid(const char *id)
{
  for (; id && *id; id++)
  {
    if (*id >= ' ' && *id <= 'z' && *id != ':')
      putchar(*id);
    else
      printf("\\x%02x", (*id) & 0xff);
  }
}

static void print_fingerprint(struct PgpKeyInfo *p)
{
  if (!p->fingerprint)
    return;

  printf("fpr:::::::::%s:\n", p->fingerprint);
}

static void pgpring_dump_signatures(struct PgpSignature *sig)
{
  for (; sig; sig = sig->next)
  {
    if (sig->sigtype == 0x10 || sig->sigtype == 0x11 || sig->sigtype == 0x12 ||
        sig->sigtype == 0x13)
    {
      printf("sig::::%08lX%08lX::::::%X:\n", sig->sid1, sig->sid2, sig->sigtype);
    }
    else if (sig->sigtype == 0x20)
    {
      printf("rev::::%08lX%08lX::::::%X:\n", sig->sid1, sig->sid2, sig->sigtype);
    }
  }
}

static void pgpring_dump_keyblock(struct PgpKeyInfo *p)
{
  for (struct tm *tp = NULL; p; p = p->next)
  {
    bool first = true;

    if (p->flags & KEYFLAG_SECRET)
    {
      if (p->flags & KEYFLAG_SUBKEY)
        printf("ssb:");
      else
        printf("sec:");
    }
    else
    {
      if (p->flags & KEYFLAG_SUBKEY)
        printf("sub:");
      else
        printf("pub:");
    }

    if (p->flags & KEYFLAG_REVOKED)
      putchar('r');
    if (p->flags & KEYFLAG_EXPIRED)
      putchar('e');
    if (p->flags & KEYFLAG_DISABLED)
      putchar('d');

    for (struct PgpUid *uid = p->address; uid; uid = uid->next, first = false)
    {
      if (!first)
      {
        printf("uid:%c::::::::", gnupg_trustletter(uid->trust));
        print_userid(uid->addr);
        printf(":\n");
      }
      else
      {
        if (p->flags & KEYFLAG_SECRET)
          putchar('u');
        else
          putchar(gnupg_trustletter(uid->trust));

        const time_t t = p->gen_time;
        tp = gmtime(&t);

        printf(":%d:%d:%s:%04d-%02d-%02d::::", p->keylen, p->numalg, p->keyid,
               1900 + tp->tm_year, tp->tm_mon + 1, tp->tm_mday);

        print_userid(uid->addr);
        printf("::");

        if (pgp_canencrypt(p->numalg))
          putchar('e');
        if (pgp_cansign(p->numalg))
          putchar('s');
        if (p->flags & KEYFLAG_DISABLED)
          putchar('D');
        printf(":\n");

        if (dump_fingerprints)
          print_fingerprint(p);
      }

      if (dump_signatures)
      {
        if (first)
          pgpring_dump_signatures(p->sigs);
        pgpring_dump_signatures(uid->sigs);
      }
    }
  }
}

static bool pgpring_string_matches_hint(const char *s, const char *hints[], int nhints)
{
  if (!hints || !nhints)
    return true;

  for (int i = 0; i < nhints; i++)
  {
    if (mutt_str_stristr(s, hints[i]) != NULL)
      return true;
  }

  return false;
}

/**
 * pgp_make_pgp2_fingerprint - The actual key ring parser
 */
static void pgp_make_pgp2_fingerprint(unsigned char *buf, unsigned char *digest)
{
  struct Md5Ctx ctx;
  size_t size = 0;

  mutt_md5_init_ctx(&ctx);

  size = (buf[0] << 8) + buf[1];
  size = ((size + 7) / 8);
  buf = &buf[2];

  mutt_md5_process_bytes(buf, size, &ctx);

  buf = &buf[size];

  size = (buf[0] << 8) + buf[1];
  size = ((size + 7) / 8);
  buf = &buf[2];

  mutt_md5_process_bytes(buf, size, &ctx);

  mutt_md5_finish_ctx(&ctx, digest);
}

static char *binary_fingerprint_to_string(unsigned char *buf, size_t length)
{
  char *fingerprint = mutt_mem_malloc((length * 2) + 1);
  char *pf = fingerprint;

  for (size_t i = 0; i < length; i++)
  {
    sprintf(pf, "%02X", buf[i]);
    pf += 2;
  }
  *pf = 0;

  return fingerprint;
}

static struct PgpKeyInfo *pgp_parse_pgp2_key(unsigned char *buf, size_t l)
{
  struct PgpKeyInfo *p = NULL;
  unsigned char alg;
  unsigned char digest[MD5_DIGEST_LENGTH];
  size_t expl;
  unsigned long id;
  time_t gen_time = 0;
  unsigned short exp_days = 0;
  size_t j;
  int i;
  unsigned char scratch[LONG_STRING];

  if (l < 12)
    return NULL;

  p = pgp_new_keyinfo();

  for (i = 0, j = 2; i < 4; i++)
    gen_time = (gen_time << 8) + buf[j++];

  p->gen_time = gen_time;

  for (i = 0; i < 2; i++)
    exp_days = (exp_days << 8) + buf[j++];

  if (exp_days && time(NULL) > gen_time + exp_days * 24 * 3600)
    p->flags |= KEYFLAG_EXPIRED;

  alg = buf[j++];

  p->numalg = alg;
  p->algorithm = pgp_pkalgbytype(alg);
  p->flags |= pgp_get_abilities(alg);

  if (dump_fingerprints)
  {
    /* j now points to the key material, which we need for the fingerprint */
    pgp_make_pgp2_fingerprint(&buf[j], digest);
    p->fingerprint = binary_fingerprint_to_string(digest, MD5_DIGEST_LENGTH);
  }

  expl = 0;
  for (i = 0; i < 2; i++)
    expl = (expl << 8) + buf[j++];

  p->keylen = expl;

  expl = (expl + 7) / 8;
  if (expl < 4)
    goto bailout;

  j += expl - 8;

  for (int k = 0; k < 2; k++)
  {
    for (id = 0, i = 0; i < 4; i++)
      id = (id << 8) + buf[j++];

    snprintf((char *) scratch + k * 8, sizeof(scratch) - k * 8, "%08lX", id);
  }

  p->keyid = mutt_str_strdup((char *) scratch);

  return p;

bailout:

  FREE(&p);
  return NULL;
}

static void pgp_make_pgp3_fingerprint(unsigned char *buf, size_t l, unsigned char *digest)
{
  unsigned char dummy;
  struct Sha1Ctx context;

  mutt_sha1_init(&context);

  dummy = buf[0] & 0x3f;

  if (dummy == PT_SUBSECKEY || dummy == PT_SUBKEY || dummy == PT_SECKEY)
    dummy = PT_PUBKEY;

  dummy = (dummy << 2) | 0x81;
  mutt_sha1_update(&context, &dummy, 1);
  dummy = ((l - 1) >> 8) & 0xff;
  mutt_sha1_update(&context, &dummy, 1);
  dummy = (l - 1) & 0xff;
  mutt_sha1_update(&context, &dummy, 1);
  mutt_sha1_update(&context, buf + 1, l - 1);
  mutt_sha1_final(digest, &context);
}

static void skip_bignum(unsigned char *buf, size_t l, size_t j, size_t *toff, size_t n)
{
  do
  {
    const size_t len = (buf[j] << 8) + buf[j + 1];
    j += (len + 7) / 8 + 2;
  } while (j <= l && --n > 0);

  if (toff)
    *toff = j;
}

static struct PgpKeyInfo *pgp_parse_pgp3_key(unsigned char *buf, size_t l)
{
  unsigned char alg;
  unsigned char digest[SHA_DIGEST_LENGTH];
  unsigned char scratch[LONG_STRING];
  time_t gen_time = 0;
  unsigned long id;
  int i;
  short len;
  size_t j;

  struct PgpKeyInfo *p = pgp_new_keyinfo();
  j = 2;

  for (i = 0; i < 4; i++)
    gen_time = (gen_time << 8) + buf[j++];

  p->gen_time = gen_time;

  alg = buf[j++];

  p->numalg = alg;
  p->algorithm = pgp_pkalgbytype(alg);
  p->flags |= pgp_get_abilities(alg);

  len = (buf[j] << 8) + buf[j + 1];
  p->keylen = len;

  if (alg >= 1 && alg <= 3)
    skip_bignum(buf, l, j, &j, 2);
  else if (alg == 16 || alg == 20)
    skip_bignum(buf, l, j, &j, 3);
  else if (alg == 17)
    skip_bignum(buf, l, j, &j, 4);

  pgp_make_pgp3_fingerprint(buf, j, digest);
  if (dump_fingerprints)
  {
    p->fingerprint = binary_fingerprint_to_string(digest, SHA_DIGEST_LENGTH);
  }

  for (int k = 0; k < 2; k++)
  {
    for (id = 0, i = SHA_DIGEST_LENGTH - 8 + k * 4;
         i < SHA_DIGEST_LENGTH + (k - 1) * 4; i++)
    {
      id = (id << 8) + digest[i];
    }

    snprintf((char *) scratch + k * 8, sizeof(scratch) - k * 8, "%08lX", id);
  }

  p->keyid = mutt_str_strdup((char *) scratch);

  return p;
}

static struct PgpKeyInfo *pgp_parse_keyinfo(unsigned char *buf, size_t l)
{
  if (!buf || l < 2)
    return NULL;

  switch (buf[1])
  {
    case 2:
    case 3:
      return pgp_parse_pgp2_key(buf, l);
    case 4:
      return pgp_parse_pgp3_key(buf, l);
    default:
      return NULL;
  }
}

static int pgp_parse_pgp2_sig(unsigned char *buf, size_t l,
                              struct PgpKeyInfo *p, struct PgpSignature *s)
{
  unsigned char sigtype;
  time_t sig_gen_time;
  unsigned long signerid1;
  unsigned long signerid2;
  size_t j;

  if (l < 22)
    return -1;

  j = 3;
  sigtype = buf[j++];

  sig_gen_time = 0;
  for (int i = 0; i < 4; i++)
    sig_gen_time = (sig_gen_time << 8) + buf[j++];

  signerid1 = signerid2 = 0;
  for (int i = 0; i < 4; i++)
    signerid1 = (signerid1 << 8) + buf[j++];

  for (int i = 0; i < 4; i++)
    signerid2 = (signerid2 << 8) + buf[j++];

  if (sigtype == 0x20 || sigtype == 0x28)
    p->flags |= KEYFLAG_REVOKED;

  if (s)
  {
    s->sigtype = sigtype;
    s->sid1 = signerid1;
    s->sid2 = signerid2;
  }

  return 0;
}

static int pgp_parse_pgp3_sig(unsigned char *buf, size_t l,
                              struct PgpKeyInfo *p, struct PgpSignature *s)
{
  time_t sig_gen_time = -1;
  long validity = -1;
  long key_validity = -1;
  unsigned long signerid1 = 0;
  unsigned long signerid2 = 0;
  size_t j;
  short have_critical_spks = 0;

  if (l < 7)
    return -1;

  j = 2;

  const unsigned char sigtype = buf[j++];
  j += 2; /* pkalg, hashalg */

  for (short ii = 0; ii < 2; ii++)
  {
    size_t ml = (buf[j] << 8) + buf[j + 1];
    j += 2;

    if (j + ml > l)
      break;

    size_t nextone = j;
    while (ml)
    {
      j = nextone;
      size_t skl = buf[j++];
      if (!--ml)
        break;

      if (skl >= 192)
      {
        skl = (skl - 192) * 256 + buf[j++] + 192;
        if (!--ml)
          break;
      }

      if ((int) ml - (int) skl < 0)
        break;
      ml -= skl;

      nextone = j + skl;
      const unsigned char skt = buf[j++];

      switch (skt & 0x7f)
      {
        case 2: /* creation time */
        {
          if (skl < 4)
            break;
          sig_gen_time = 0;
          for (int i = 0; i < 4; i++)
            sig_gen_time = (sig_gen_time << 8) + buf[j++];

          break;
        }
        case 3: /* expiration time */
        {
          if (skl < 4)
            break;
          validity = 0;
          for (int i = 0; i < 4; i++)
            validity = (validity << 8) + buf[j++];
          break;
        }
        case 9: /* key expiration time */
        {
          if (skl < 4)
            break;
          key_validity = 0;
          for (int i = 0; i < 4; i++)
            key_validity = (key_validity << 8) + buf[j++];
          break;
        }
        case 16: /* issuer key ID */
        {
          if (skl < 8)
            break;
          signerid2 = signerid1 = 0;
          for (int i = 0; i < 4; i++)
            signerid1 = (signerid1 << 8) + buf[j++];
          for (int i = 0; i < 4; i++)
            signerid2 = (signerid2 << 8) + buf[j++];

          break;
        }
        case 10: /* CMR key */
          break;
        case 4:  /* exportable */
        case 5:  /* trust */
        case 6:  /* regex */
        case 7:  /* revocable */
        case 11: /* Pref. symm. alg. */
        case 12: /* revocation key */
        case 20: /* notation data */
        case 21: /* pref. hash */
        case 22: /* pref. comp.alg. */
        case 23: /* key server prefs. */
        case 24: /* pref. key server */
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
  if (key_validity != -1 && time(NULL) > p->gen_time + key_validity)
    p->flags |= KEYFLAG_EXPIRED;
  if (have_critical_spks)
    p->flags |= KEYFLAG_CRITICAL;

  if (s)
  {
    s->sigtype = sigtype;
    s->sid1 = signerid1;
    s->sid2 = signerid2;
  }

  return 0;
}

static int pgp_parse_sig(unsigned char *buf, size_t l, struct PgpKeyInfo *p,
                         struct PgpSignature *sig)
{
  if (!buf || l < 2 || !p)
    return -1;

  switch (buf[1])
  {
    case 2:
    case 3:
      return pgp_parse_pgp2_sig(buf, l, p, sig);
    case 4:
      return pgp_parse_pgp3_sig(buf, l, p, sig);
    default:
      return -1;
  }
}

/* parse one key block, including all subkeys. */

static struct PgpKeyInfo *pgp_parse_keyblock(FILE *fp)
{
  unsigned char *buf = NULL;
  unsigned char pt = 0;
  size_t l = 0;
  short err = 0;

  fpos_t pos;

  struct PgpKeyInfo *root = NULL;
  struct PgpKeyInfo **last = &root;
  struct PgpKeyInfo *p = NULL;
  struct PgpUid *uid = NULL;
  struct PgpUid **addr = NULL;
  struct PgpSignature **lsig = NULL;

  fgetpos(fp, &pos);

  while (!err && (buf = pgp_read_packet(fp, &l)) != NULL)
  {
    unsigned char last_pt = pt;
    pt = buf[0] & 0x3f;

    /* check if we have read the complete key block. */

    if ((pt == PT_SECKEY || pt == PT_PUBKEY) && root)
    {
      fsetpos(fp, &pos);
      return root;
    }

    switch (pt)
    {
      case PT_SECKEY:
      case PT_PUBKEY:
      case PT_SUBKEY:
      case PT_SUBSECKEY:
      {
        *last = p = pgp_parse_keyinfo(buf, l);
        if (!*last)
        {
          err = 1;
          break;
        }

        last = &p->next;
        addr = &p->address;
        lsig = &p->sigs;

        if (pt == PT_SUBKEY || pt == PT_SUBSECKEY)
        {
          p->flags |= KEYFLAG_SUBKEY;
          if (root && (p != root))
          {
            p->parent = root;
            p->address = pgp_copy_uids(root->address, p);
            while (*addr)
              addr = &(*addr)->next;
          }
        }

        if (pt == PT_SECKEY || pt == PT_SUBSECKEY)
          p->flags |= KEYFLAG_SECRET;

        break;
      }

      case PT_SIG:
      {
        if (lsig)
        {
          struct PgpSignature *signature =
              mutt_mem_calloc(1, sizeof(struct PgpSignature));
          *lsig = signature;
          lsig = &signature->next;

          pgp_parse_sig(buf, l, p, signature);
        }
        break;
      }

      case PT_TRUST:
      {
        if (p && (last_pt == PT_SECKEY || last_pt == PT_PUBKEY ||
                  last_pt == PT_SUBKEY || last_pt == PT_SUBSECKEY))
        {
          if (buf[1] & 0x20)
          {
            p->flags |= KEYFLAG_DISABLED;
          }
        }
        else if (last_pt == PT_NAME && uid)
        {
          uid->trust = buf[1];
        }
        break;
      }
      case PT_NAME:
      {
        char *chr = NULL;

        if (!addr)
          break;

        chr = mutt_mem_malloc(l);
        if (l > 0)
        {
          memcpy(chr, buf + 1, l - 1);
          chr[l - 1] = '\0';
        }

        *addr = uid = mutt_mem_calloc(1, sizeof(struct PgpUid)); /* XXX */
        uid->addr = chr;
        uid->parent = p;
        uid->trust = 0;
        addr = &uid->next;
        lsig = &uid->sigs;

        /* the following tags are generated by
         * pgp 2.6.3in.
         */

        if (strstr(chr, "ENCR"))
          p->flags |= KEYFLAG_PREFER_ENCRYPTION;
        if (strstr(chr, "SIGN"))
          p->flags |= KEYFLAG_PREFER_SIGNING;

        break;
      }
    }

    fgetpos(fp, &pos);
  }

  if (err)
    pgp_free_key(&root);

  return root;
}

/*
 * Go through the key ring file and look for keys with
 * matching IDs.
 */

static void pgpring_find_candidates(char *ringfile, const char *hints[], int nhints)
{
  fpos_t pos, keypos;

  unsigned char *buf = NULL;
  size_t l = 0;

  short err = 0;

  FILE *rfp = fopen(ringfile, "r");
  if (!rfp)
  {
    size_t error_buf_len = sizeof("fopen: ") - 1 + strlen(ringfile) + 1;
    char *error_buf = mutt_mem_malloc(error_buf_len);
    snprintf(error_buf, error_buf_len, "fopen: %s", ringfile);
    perror(error_buf);
    FREE(&error_buf);
    return;
  }

  fgetpos(rfp, &pos);
  fgetpos(rfp, &keypos);

  while (!err && (buf = pgp_read_packet(rfp, &l)) != NULL)
  {
    const unsigned char pt = buf[0] & 0x3f;

    if (l < 1)
      continue;

    if ((pt == PT_SECKEY) || (pt == PT_PUBKEY))
    {
      keypos = pos;
    }
    else if (pt == PT_NAME)
    {
      char *tmp = mutt_mem_malloc(l);

      memcpy(tmp, buf + 1, l - 1);
      tmp[l - 1] = '\0';

      if (pgpring_string_matches_hint(tmp, hints, nhints))
      {
        fsetpos(rfp, &keypos);

        /* Not bailing out here would lead us into an endless loop. */

        struct PgpKeyInfo *p = pgp_parse_keyblock(rfp);
        if (!p)
          err = 1;

        pgpring_dump_keyblock(p);
        pgp_free_key(&p);
      }

      FREE(&tmp);
    }

    fgetpos(rfp, &pos);
  }

  mutt_file_fclose(&rfp);
}

int main(int argc, char *const argv[])
{
  int c;

  short version = 2;
  short secring = 0;

  const char *tmp_kring = NULL;
  char kring[_POSIX_PATH_MAX];

  while ((c = getopt(argc, argv, "f25sk:S")) != EOF)
  {
    switch (c)
    {
      case 'S':
      {
        dump_signatures = 1;
        break;
      }

      case 'f':
      {
        dump_fingerprints = 1;
        break;
      }

      case 'k':
      {
        tmp_kring = optarg;
        break;
      }

      case '2':
      case '5':
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
        fprintf(
            stderr,
            "usage: %s [-k <key ring> | [-2 | -5] [ -s] [-S] [-f]] [hints]\n", argv[0]);
        exit(1);
      }
    }
  }

  if (tmp_kring)
    mutt_str_strfcpy(kring, tmp_kring, sizeof(kring));
  else
  {
    const char *env_home = NULL;
    char pgppath[_POSIX_PATH_MAX];
    const char *env_pgppath = mutt_str_getenv("PGPPATH");
    if (env_pgppath)
      mutt_str_strfcpy(pgppath, env_pgppath, sizeof(pgppath));
    else if ((env_home = mutt_str_getenv("HOME")))
      snprintf(pgppath, sizeof(pgppath), "%s/.pgp", env_home);
    else
    {
      fprintf(stderr, "%s: Can't determine your PGPPATH.\n", argv[0]);
      exit(1);
    }

    if (secring)
      snprintf(kring, sizeof(kring), "%s/secring.%s", pgppath, version == 2 ? "pgp" : "skr");
    else
      snprintf(kring, sizeof(kring), "%s/pubring.%s", pgppath, version == 2 ? "pgp" : "pkr");
  }

  pgpring_find_candidates(kring, (const char **) argv + optind, argc - optind);

  return 0;
}
