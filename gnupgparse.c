/*
 * Copyright (C) 1998-2000 Werner Koch <werner.koch@guug.de>
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@guug.de>
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
 *     Software Foundation, Inc., 59 Temple Place - Suite 330,
 *     Boston, MA  02111, USA.
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
      else if (ob-buf == n && (buf[n] = 0, strlen (buf) < n))
	memcpy (uid, buf, n);
    }
    safe_free ((void **) &buf);
    iconv_close (cd);
  }
}

static pgp_key_t *parse_pub_line (char *buf, int *is_subkey, pgp_key_t *k)
{
  pgp_uid_t *uid = NULL;
  int field = 0, is_uid = 0;
  char *pend, *p;
  int trust = 0;
  int flags = 0;

  *is_subkey = 0;
  if (!*buf)
    return NULL;
  
  dprint (2, (debugfile, "parse_pub_line: buf = `%s'\n", buf));
  
  for (p = buf; p; p = pend)
  {
    if ((pend = strchr (p, ':')))
      *pend++ = 0;
    field++;
    if (field > 1 && !*p)
      continue;

    switch (field)
    {
      case 1:			/* record type */
      {
	dprint (2, (debugfile, "record type: %s\n", p));
	
	if (!mutt_strcmp (p, "pub"))
	  ;
	else if (!mutt_strcmp (p, "sub"))
	  *is_subkey = 1;
	else if (!mutt_strcmp (p, "sec"))
	  ;
	else if (!mutt_strcmp (p, "ssb"))
	  *is_subkey = 1;
	else if (!mutt_strcmp (p, "uid"))
	  is_uid = 1;
	else
	  return NULL;
	
	if (!(is_uid || (*is_subkey && option (OPTPGPIGNORESUB))))
	  k = safe_calloc (sizeof (pgp_key_t), 1);

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
	  k->flags |= flags;

	break;
      }
      case 3:			/* key length  */
      {
	
	dprint (2, (debugfile, "key len: %s\n", p));
	
	if (!(*is_subkey && option (OPTPGPIGNORESUB)))
	  k->keylen = atoi (p);	/* fixme: add validation checks */
	break;
      }
      case 4:			/* pubkey algo */
      {
	
	dprint (2, (debugfile, "pubkey algorithm: %s\n", p));
	
	if (!(*is_subkey && option (OPTPGPIGNORESUB)))
	{
	  k->numalg = atoi (p);
	  k->algorithm = pgp_pkalgbytype (atoi (p));
	}

	k->flags |= pgp_get_abilities (atoi (p));
	break;
      }
      case 5:			/* 16 hex digits with the long keyid. */
      {
	dprint (2, (debugfile, "key id: %s\n", p));
	
	if (!(*is_subkey && option (OPTPGPIGNORESUB)))
	  mutt_str_replace (&k->keyid, p);
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
	time.tm_year = atoi (tstr)-1900;
	tstr[7] = '\0';
	time.tm_mon = (atoi (tstr+5))-1;
	time.tm_mday = atoi (tstr+8);
	k->gen_time = mutt_mktime (&time, 0);
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
	if (!pend || !*p)
	  break;			/* empty field or no trailing colon */

	/* ignore user IDs on subkeys */
	if (!is_uid && (*is_subkey && option (OPTPGPIGNORESUB)))
	  break;
	
	dprint (2, (debugfile, "user ID: %s\n", p));
	
	uid = safe_calloc (sizeof (pgp_uid_t), 1);
	fix_uid (p);
	uid->addr = safe_strdup (p);
	uid->trust = trust;
	uid->flags |= flags;
	uid->parent = k;
	uid->next = k->address;
	k->address = uid;
	
	if (strstr (p, "ENCR"))
	  k->flags |= KEYFLAG_PREFER_ENCRYPTION;
	if (strstr (p, "SIGN"))
	  k->flags |= KEYFLAG_PREFER_SIGNING;

	break;
      }
      case 11:			/* signature class  */
        break;
      case 12:			/* key capabilities */
        break;
      default:
        break;
    }
  }
  return k;
}

pgp_key_t *pgp_get_candidates (pgp_ring_t keyring, LIST * hints)
{
  FILE *fp;
  pid_t thepid;
  char buf[LONG_STRING];
  pgp_key_t *db = NULL, **kend, *k = NULL, *kk, *mainkey = NULL;
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

  fclose (fp);
  mutt_wait_filter (thepid);

  close (devnull);
  
  return db;
}

