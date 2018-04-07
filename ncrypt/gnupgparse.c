/**
 * @file
 * Parse the output of CLI PGP program
 *
 * @authors
 * Copyright (C) 1998-2000,2003 Werner Koch <werner.koch@guug.de>
 * Copyright (C) 1999-2003 Thomas Roessler <roessler@does-not-exist.org>
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

/* NOTE: This code used to be the parser for GnuPG's output.
 *
 * Nowadays, we are using an external pubring lister with PGP which mimics
 * gpg's output format.
 */

#include "config.h"
#include <ctype.h>
#include <fcntl.h>
#include <iconv.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "filter.h"
#include "globals.h"
#include "ncrypt.h"
#include "options.h"
#include "pgpinvoke.h"
#include "pgpkey.h"
#include "pgplib.h"
#include "protos.h"

/****************
 * Read the GNUPG keys.  For now we read the complete keyring by
 * calling gnupg in a special mode.
 *
 * The output format of gpgm is colon delimited with these fields:
 *   - record type ("pub","uid","sig","rev" etc.)
 *   - trust info
 *   - key length
 *   - pubkey algo
 *   - 16 hex digits with the long keyid.
 *   - timestamp (1998-02-28)
 *   - Local id
 *   - ownertrust
 *   - name
 *   - signature class
 */

/* decode the backslash-escaped user ids. */

static char *chs = NULL;

static void fix_uid(char *uid)
{
  char *s = NULL, *d = NULL;
  iconv_t cd;

  for (s = d = uid; *s;)
  {
    if (*s == '\\' && *(s + 1) == 'x' && isxdigit((unsigned char) *(s + 2)) &&
        isxdigit((unsigned char) *(s + 3)))
    {
      *d++ = hexval(*(s + 2)) << 4 | hexval(*(s + 3));
      s += 4;
    }
    else
      *d++ = *s++;
  }
  *d = '\0';

  if (chs && (cd = mutt_ch_iconv_open(chs, "utf-8", 0)) != (iconv_t) -1)
  {
    int n = s - uid + 1; /* chars available in original buffer */

    char *buf = mutt_mem_malloc(n + 1);
    const char *ib = uid;
    size_t ibl = d - uid + 1;
    char *ob = buf;
    size_t obl = n;
    iconv(cd, (ICONV_CONST char **) &ib, &ibl, &ob, &obl);
    if (!ibl)
    {
      if (ob - buf < n)
      {
        memcpy(uid, buf, ob - buf);
        uid[ob - buf] = '\0';
      }
      else if (n >= 0 && ob - buf == n && (buf[n] = 0, strlen(buf) < (size_t) n))
        memcpy(uid, buf, n);
    }
    FREE(&buf);
    iconv_close(cd);
  }
}

static struct PgpKeyInfo *parse_pub_line(char *buf, int *is_subkey, struct PgpKeyInfo *k)
{
  struct PgpUid *uid = NULL;
  int field = 0;
  bool is_uid = false;
  bool is_pub = false;
  bool is_fpr = false;
  char *pend = NULL, *p = NULL;
  int trust = 0;
  int flags = 0;
  struct PgpKeyInfo tmp;
  char tstr[11];

  *is_subkey = 0;
  if (!*buf)
    return NULL;

  /* if we're given a key, merge our parsing results, else
   * start with a fresh one to work with so that we don't
   * mess up the real key in case we find parsing errors. */
  if (k)
    memcpy(&tmp, k, sizeof(tmp));
  else
    memset(&tmp, 0, sizeof(tmp));

  mutt_debug(2, "buf = '%s'\n", buf);

  for (p = buf; p; p = pend)
  {
    pend = strchr(p, ':');
    if (pend)
      *pend++ = 0;
    field++;
    if (!*p && (field != 1) && (field != 10))
      continue;

    if (is_fpr && (field != 10))
      continue;

    switch (field)
    {
      case 1: /* record type */
      {
        mutt_debug(2, "record type: %s\n", p);

        if (mutt_str_strcmp(p, "pub") == 0)
          is_pub = true;
        else if (mutt_str_strcmp(p, "sub") == 0)
          *is_subkey = 1;
        else if (mutt_str_strcmp(p, "sec") == 0)
          ;
        else if (mutt_str_strcmp(p, "ssb") == 0)
          *is_subkey = 1;
        else if (mutt_str_strcmp(p, "uid") == 0)
          is_uid = true;
        else if (mutt_str_strcmp(p, "fpr") == 0)
          is_fpr = true;
        else
          return NULL;

        if (!(is_uid || is_fpr || (*is_subkey && PgpIgnoreSubkeys)))
          memset(&tmp, 0, sizeof(tmp));

        break;
      }
      case 2: /* trust info */
      {
        mutt_debug(2, "trust info: %s\n", p);

        switch (*p)
        { /* look only at the first letter */
          case 'd':
            flags |= KEYFLAG_DISABLED;
            break;
          case 'e':
            flags |= KEYFLAG_EXPIRED;
            break;
          case 'f':
            trust = 3;
            break;
          case 'm':
            trust = 2;
            break;
          case 'n':
            trust = 1;
            break;
          case 'r':
            flags |= KEYFLAG_REVOKED;
            break;
          case 'u':
            trust = 3;
            break;
        }

        if (!is_uid && !(*is_subkey && PgpIgnoreSubkeys))
          tmp.flags |= flags;

        break;
      }
      case 3: /* key length  */
      {
        mutt_debug(2, "key len: %s\n", p);

        if (!(*is_subkey && PgpIgnoreSubkeys) && mutt_str_atos(p, &tmp.keylen) < 0)
        {
          goto bail;
        }
        break;
      }
      case 4: /* pubkey algo */
      {
        mutt_debug(2, "pubkey algorithm: %s\n", p);

        if (!(*is_subkey && PgpIgnoreSubkeys))
        {
          int x = 0;
          if (mutt_str_atoi(p, &x) < 0)
            goto bail;
          tmp.numalg = x;
          tmp.algorithm = pgp_pkalgbytype(x);
        }
        break;
      }
      case 5: /* 16 hex digits with the long keyid. */
      {
        mutt_debug(2, "key id: %s\n", p);

        if (!(*is_subkey && PgpIgnoreSubkeys))
          mutt_str_replace(&tmp.keyid, p);
        break;
      }
      case 6: /* timestamp (1998-02-28) */
      {
        struct tm time;

        mutt_debug(2, "time stamp: %s\n", p);

        if (!p)
          break;
        time.tm_sec = 0;
        time.tm_min = 0;
        time.tm_hour = 12;
        strncpy(tstr, p, 11);
        tstr[4] = '\0';
        tstr[7] = '\0';
        if (mutt_str_atoi(tstr, &time.tm_year) < 0)
        {
          p = tstr;
          goto bail;
        }
        time.tm_year -= 1900;
        if (mutt_str_atoi(tstr + 5, &time.tm_mon) < 0)
        {
          p = tstr + 5;
          goto bail;
        }
        time.tm_mon -= 1;
        if (mutt_str_atoi(tstr + 8, &time.tm_mday) < 0)
        {
          p = tstr + 8;
          goto bail;
        }
        tmp.gen_time = mutt_date_make_time(&time, 0);
        break;
      }
      case 7: /* valid for n days */
        break;
      case 8: /* Local id         */
        break;
      case 9: /* ownertrust       */
        break;
      case 10: /* name             */
      {
        /* Empty field or no trailing colon.
         * We allow an empty field for a pub record type because it is
         * possible for a primary uid record to have an empty User-ID
         * field.  Without any address records, it is not possible to
         * use the key in neomutt.
         */
        if (!(pend && (*p || is_pub)))
          break;

        if (is_fpr)
        {
          /* don't let a subkey fpr overwrite an existing primary key fpr */
          if (!tmp.fingerprint)
            tmp.fingerprint = mutt_str_strdup(p);
          break;
        }

        /* ignore user IDs on subkeys */
        if (!is_uid && (*is_subkey && PgpIgnoreSubkeys))
          break;

        mutt_debug(2, "user ID: %s\n", NONULL(p));

        uid = mutt_mem_calloc(1, sizeof(struct PgpUid));
        fix_uid(p);
        uid->addr = mutt_str_strdup(p);
        uid->trust = trust;
        uid->flags |= flags;
        uid->next = tmp.address;
        tmp.address = uid;

        if (strstr(p, "ENCR"))
          tmp.flags |= KEYFLAG_PREFER_ENCRYPTION;
        if (strstr(p, "SIGN"))
          tmp.flags |= KEYFLAG_PREFER_SIGNING;

        break;
      }
      case 11: /* signature class  */
        break;
      case 12: /* key capabilities */
        mutt_debug(2, "capabilities info: %s\n", p);

        while (*p)
        {
          switch (*p++)
          {
            case 'D':
              flags |= KEYFLAG_DISABLED;
              break;

            case 'e':
              flags |= KEYFLAG_CANENCRYPT;
              break;

            case 's':
              flags |= KEYFLAG_CANSIGN;
              break;
          }
        }

        if (!is_uid && (!*is_subkey || !PgpIgnoreSubkeys ||
                        !((flags & KEYFLAG_DISABLED) || (flags & KEYFLAG_REVOKED) ||
                          (flags & KEYFLAG_EXPIRED))))
          tmp.flags |= flags;

        break;

      default:
        break;
    }
  }

  /* merge temp key back into real key */
  if (!(is_uid || is_fpr || (*is_subkey && PgpIgnoreSubkeys)))
    k = mutt_mem_malloc(sizeof(*k));
  memcpy(k, &tmp, sizeof(*k));
  /* fixup parentship of uids after merging the temp key into
   * the real key */
  if (tmp.address)
  {
    for (uid = k->address; uid; uid = uid->next)
      uid->parent = k;
  }

  return k;

bail:
  mutt_debug(5, "invalid number: '%s'\n", p);
  return NULL;
}

/**
 * pgp_get_candidates - Find PGP keys matching a list of hints
 * @param keyring PGP Keyring
 * @param hints   List of strings to match
 * @retval ptr  Key list
 * @retval NULL Error
 */
struct PgpKeyInfo *pgp_get_candidates(enum PgpRing keyring, struct ListHead *hints)
{
  FILE *fp = NULL;
  pid_t thepid;
  char buf[LONG_STRING];
  struct PgpKeyInfo *db = NULL, **kend = NULL, *k = NULL, *kk = NULL, *mainkey = NULL;
  int is_sub;
  int devnull;

  devnull = open("/dev/null", O_RDWR);
  if (devnull == -1)
    return NULL;

  mutt_str_replace(&chs, Charset);

  thepid = pgp_invoke_list_keys(NULL, &fp, NULL, -1, -1, devnull, keyring, hints);
  if (thepid == -1)
  {
    close(devnull);
    return NULL;
  }

  kend = &db;
  k = NULL;
  while (fgets(buf, sizeof(buf) - 1, fp))
  {
    kk = parse_pub_line(buf, &is_sub, k);
    if (!kk)
      continue;

    /* Only append kk to the list if it's new. */
    if (kk != k)
    {
      if (k)
        kend = &k->next;
      *kend = k = kk;

      if (is_sub)
      {
        struct PgpUid **l = NULL;

        k->flags |= KEYFLAG_SUBKEY;
        k->parent = mainkey;
        for (l = &k->address; *l; l = &(*l)->next)
          ;
        *l = pgp_copy_uids(mainkey->address, k);
      }
      else
        mainkey = k;
    }
  }

  if (ferror(fp))
    mutt_perror("fgets");

  mutt_file_fclose(&fp);
  mutt_wait_filter(thepid);

  close(devnull);

  return db;
}
