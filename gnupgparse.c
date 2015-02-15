/*
 * Copyright (C) 1998-2000,2003 Werner Koch <werner.koch@guug.de>
 * Copyright (C) 1999-2003 Thomas Roessler <roessler@does-not-exist.org>
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
 *     Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *     Boston, MA  02110-1301, USA.
 */

/*
 * NOTE
 * 
 * This code used to be the parser for GnuPG's output.
 * 
 * Nowadays, we are using an external pubring lister with PGP which mimics 
 * gpg's output format.
 * 
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>

#include "mutt.h"
#include "pgp.h"
#include "charset.h"

/* for hexval */
#include "mime.h"

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

static char *_chs = 0;

static void fix_uid (char *uid)
{
  char *s, *d;
  iconv_t cd;

  for (s = d = uid; *s;)
  {
    if (*s == '\\' && *(s+1) == 'x' && isxdigit ((unsigned char) *(s+2)) && isxdigit ((unsigned char) *(s+3)))
    {
      *d++ = hexval (*(s+2)) << 4 | hexval (*(s+3));
      s += 4;
    }
    else
      *d++ = *s++;
  }
  *d = '\0';

  if (_chs && (cd = mutt_iconv_open (_chs, "utf-8", 0)) != (iconv_t)-1)
  {
    int n = s - uid + 1; /* chars available in original buffer */
    char *buf;
    ICONV_CONST char *ib;
    char *ob;
    size_t ibl, obl;

    buf = safe_malloc (n+1);
    ib = uid, ibl = d - uid + 1, ob = buf, obl = n;
    iconv (cd, &ib, &ibl, &ob, &obl);
    if (!ibl)
    {
      if (ob-buf < n)
      {
	memcpy (uid, buf, ob-buf);
	uid[ob-buf] = '\0';
      }
      else if (n >= 0 && ob-buf == n && (buf[n] = 0, strlen (buf) < (size_t)n))
	memcpy (uid, buf, n);
    }
    FREE (&buf);
    iconv_close (cd);
  }
}

static pgp_key_t parse_pub_line (char *buf, int *is_subkey, pgp_key_t k)
{
  pgp_uid_t *uid = NULL;
  int field = 0, is_uid = 0;
  int is_pub = 0;
  int is_fpr = 0;
  char *pend, *p;
  int trust = 0;
  int flags = 0;
  struct pgp_keyinfo tmp;

  *is_subkey = 0;
  if (!*buf)
    return NULL;

  /* if we're given a key, merge our parsing results, else
   * start with a fresh one to work with so that we don't
   * mess up the real key in case we find parsing errors. */
  if (k)
    memcpy (&tmp, k, sizeof (tmp));
  else
    memset (&tmp, 0, sizeof (tmp));

  dprint (2, (debugfile, "parse_pub_line: buf = `%s'\n", buf));

  for (p = buf; p; p = pend)
  {
    if ((pend = strchr (p, ':')))
      *pend++ = 0;
    field++;
    if (!*p && (field != 1) && (field != 10))
      continue;

    if (is_fpr && (field != 10))
      continue;

    switch (field)
    {
      case 1:			/* record type */
      {
	dprint (2, (debugfile, "record type: %s\n", p));

	if (!mutt_strcmp (p, "pub"))
	  is_pub = 1;
	else if (!mutt_strcmp (p, "sub"))
	  *is_subkey = 1;
	else if (!mutt_strcmp (p, "sec"))
	  ;
	else if (!mutt_strcmp (p, "ssb"))
	  *is_subkey = 1;
	else if (!mutt_strcmp (p, "uid"))
	  is_uid = 1;
	else if (!mutt_strcmp (p, "fpr"))
	  is_fpr = 1;
	else
	  return NULL;

	if (!(is_uid || is_fpr || (*is_subkey && option (OPTPGPIGNORESUB))))
	  memset (&tmp, 0, sizeof (tmp));

	break;
      }
      case 2:			/* trust info */
      {
	dprint (2, (debugfile, "trust info: %s\n", p));

	switch (*p)
	{				/* look only at the first letter */
	  case 'e':
	    flags |= KEYFLAG_EXPIRED;
	    break;
	  case 'r':
	    flags |= KEYFLAG_REVOKED;
	    break;
	  case 'd':
	    flags |= KEYFLAG_DISABLED;
	    break;
	  case 'n':
	    trust = 1;
	    break;
	  case 'm':
	    trust = 2;
	    break;
	  case 'f':
	    trust = 3;
	    break;
	  case 'u':
	    trust = 3;
	    break;
	}

        if (!is_uid && !(*is_subkey && option (OPTPGPIGNORESUB)))
	  tmp.flags |= flags;

	break;
      }
      case 3:			/* key length  */
      {
	dprint (2, (debugfile, "key len: %s\n", p));

	if (!(*is_subkey && option (OPTPGPIGNORESUB)) &&
	    mutt_atos (p, &tmp.keylen) < 0)
	  goto bail;
	break;
      }
      case 4:			/* pubkey algo */
      {
	dprint (2, (debugfile, "pubkey algorithm: %s\n", p));

	if (!(*is_subkey && option (OPTPGPIGNORESUB)))
	{
	  int x = 0;
	  if (mutt_atoi (p, &x) < 0)
	    goto bail;
	  tmp.numalg = x;
	  tmp.algorithm = pgp_pkalgbytype (x);
	}
	break;
      }
      case 5:			/* 16 hex digits with the long keyid. */
      {
	dprint (2, (debugfile, "key id: %s\n", p));

	if (!(*is_subkey && option (OPTPGPIGNORESUB)))
	  mutt_str_replace (&tmp.keyid, p);
	break;

      }
      case 6:			/* timestamp (1998-02-28) */
      {
	char tstr[11];
	struct tm time;

	dprint (2, (debugfile, "time stamp: %s\n", p));

	if (!p)
	  break;
	time.tm_sec = 0;
	time.tm_min = 0;
	time.tm_hour = 12;
	strncpy (tstr, p, 11);
	tstr[4] = '\0';
	tstr[7] = '\0';
	if (mutt_atoi (tstr, &time.tm_year) < 0)
	{
	  p = tstr;
	  goto bail;
	}
	time.tm_year -= 1900;
	if (mutt_atoi (tstr+5, &time.tm_mon) < 0)
	{
	  p = tstr+5;
	  goto bail;
	}
	time.tm_mon -= 1;
	if (mutt_atoi (tstr+8, &time.tm_mday) < 0)
	{
	  p = tstr+8;
	  goto bail;
	}
	tmp.gen_time = mutt_mktime (&time, 0);
        break;
      }
      case 7:			/* valid for n days */
        break;
      case 8:			/* Local id         */
        break;
      case 9:			/* ownertrust       */
        break;
      case 10:			/* name             */
      {
        /* Empty field or no trailing colon.
         * We allow an empty field for a pub record type because it is
         * possible for a primary uid record to have an empty User-ID
         * field.  Without any address records, it is not possible to
         * use the key in mutt.
         */
        if (!(pend && (*p || is_pub)))
	  break;

        if (is_fpr)
        {
          /* don't let a subkey fpr overwrite an existing primary key fpr */
          if (!tmp.fingerprint)
            tmp.fingerprint = safe_strdup (p);
          break;
        }

	/* ignore user IDs on subkeys */
	if (!is_uid && (*is_subkey && option (OPTPGPIGNORESUB)))
	  break;

	dprint (2, (debugfile, "user ID: %s\n", NONULL (p)));

	uid = safe_calloc (sizeof (pgp_uid_t), 1);
	fix_uid (p);
	uid->addr = safe_strdup (p);
	uid->trust = trust;
	uid->flags |= flags;
	uid->next = tmp.address;
	tmp.address = uid;

	if (strstr (p, "ENCR"))
	  tmp.flags |= KEYFLAG_PREFER_ENCRYPTION;
	if (strstr (p, "SIGN"))
	  tmp.flags |= KEYFLAG_PREFER_SIGNING;

	break;
      }
      case 11:			/* signature class  */
        break;
      case 12:			/* key capabilities */
	dprint (2, (debugfile, "capabilities info: %s\n", p));
	
	while(*p)
	  {
	    switch(*p++)
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

        if (!is_uid && 
	    (!*is_subkey || !option (OPTPGPIGNORESUB)
	     || !((flags & KEYFLAG_DISABLED)
		  || (flags & KEYFLAG_REVOKED)
		  || (flags & KEYFLAG_EXPIRED))))
	  tmp.flags |= flags;

	break;
      
      default:
        break;
    }
  }

  /* merge temp key back into real key */
  if (!(is_uid || is_fpr || (*is_subkey && option (OPTPGPIGNORESUB))))
    k = safe_malloc (sizeof (*k));
  memcpy (k, &tmp, sizeof (*k));
  /* fixup parentship of uids after mering the temp key into
   * the real key */
  if (tmp.address)
  {
    for (uid = k->address; uid; uid = uid->next)
      uid->parent = k;
  }

  return k;

bail:
  dprint(5,(debugfile,"parse_pub_line: invalid number: '%s'\n", p));
  return NULL;
}

pgp_key_t pgp_get_candidates (pgp_ring_t keyring, LIST * hints)
{
  FILE *fp;
  pid_t thepid;
  char buf[LONG_STRING];
  pgp_key_t db = NULL, *kend, k = NULL, kk, mainkey = NULL;
  int is_sub;
  int devnull;

  if ((devnull = open ("/dev/null", O_RDWR)) == -1)
    return NULL;

  mutt_str_replace (&_chs, Charset);
  
  thepid = pgp_invoke_list_keys (NULL, &fp, NULL, -1, -1, devnull,
				 keyring, hints);
  if (thepid == -1)
  {
    close (devnull);
    return NULL;
  }

  kend = &db;
  k = NULL;
  while (fgets (buf, sizeof (buf) - 1, fp))
  {
    if (!(kk = parse_pub_line (buf, &is_sub, k)))
      continue;

    /* Only append kk to the list if it's new. */
    if (kk != k)
    {
      if (k)
	kend = &k->next;
      *kend = k = kk;

      if (is_sub)
      {
	pgp_uid_t **l;
	
	k->flags  |= KEYFLAG_SUBKEY;
	k->parent  = mainkey;
	for (l = &k->address; *l; l = &(*l)->next)
	  ;
	*l = pgp_copy_uids (mainkey->address, k);
      }
      else
	mainkey = k;
    }
  }

  if (ferror (fp))
    mutt_perror ("fgets");

  safe_fclose (&fp);
  mutt_wait_filter (thepid);

  close (devnull);
  
  return db;
}

