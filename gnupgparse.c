/*
 * Copyright (C) 1998 Werner Koch <werner.koch@guug.de>
 * Copyright (C) 1999 Thomas Roessler <roessler@guug.de>
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "mutt.h"
#include "pgp.h"


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

static pgp_key_t *parse_pub_line (char *buf, int *is_subkey, pgp_key_t *k)
{
  pgp_uid_t *uid = NULL;
  int field = 0, is_uid = 0;
  char *pend, *p;
  int trust = 0;

  *is_subkey = 0;
  if (!*buf)
    return NULL;
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
	
	if (!is_uid)
	  k = safe_calloc (sizeof (pgp_key_t), 1);

	break;
      }
      case 2:			/* trust info */
      {
	switch (*p)
	{				/* look only at the first letter */
	  case 'e':
	    k->flags |= KEYFLAG_EXPIRED;
	    break;
	  case 'r':
	    k->flags |= KEYFLAG_REVOKED;
	    break;
	  
	  /* produce "undefined trust" as long as gnupg doesn't
	   * have a proper trust model.
	   */
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
	break;
      }
      case 3:			/* key length  */
      {
	k->keylen = atoi (p);	/* fixme: add validation checks */
	break;
      }
      case 4:			/* pubkey algo */
      {
	k->algorithm = pgp_pkalgbytype (atoi (p));
	k->flags |= pgp_get_abilities (atoi (p));
	break;
      }
      case 5:			/* 16 hex digits with the long keyid. */
      {
	/* We really should do a check here */
	k->keyid = safe_strdup (p);
	break;

      }
      case 6:			/* timestamp (1998-02-28) */
      {
	char tstr[11];
	struct tm time;
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
	uid = safe_calloc (sizeof (pgp_uid_t), 1);
	uid->addr = safe_strdup (p);
	uid->trust = trust;
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
      default:
        break;
    }
  }
  return k;
}

static pid_t gpg_invoke_list_keys (struct pgp_vinfo *pgp,
			      FILE ** pgpin, FILE ** pgpout, FILE ** pgperr,
				   int pgpinfd, int pgpoutfd, int pgperrfd,
				   pgp_ring_t keyring,
				   LIST * hints)
{
  char cmd[HUGE_STRING];
  char tmpcmd[HUGE_STRING];

  /* we use gpgm here */
  snprintf (cmd, sizeof (cmd),
	    "%sm --no-verbose --batch --with-colons --list-%skeys ",
	    NONULL (*pgp->binary), keyring == PGP_SECRING ? "secret-" : "");

  for (; hints; hints = hints->next)
  {
    snprintf (tmpcmd, sizeof (tmpcmd), "%s %s", cmd, (char *) hints->data);
    strcpy (cmd, tmpcmd);
  }

  return mutt_create_filter_fd (cmd, pgpin, pgpout, pgperr,
				pgpinfd, pgpoutfd, pgperrfd);
}

pgp_key_t *gpg_get_candidates (struct pgp_vinfo * pgp, pgp_ring_t keyring,
			       LIST * hints)
{
  FILE *fp;
  pid_t thepid;
  char buf[LONG_STRING];
  pgp_key_t *db = NULL, **kend, *k = NULL, *kk, *mainkey = NULL;
  int is_sub;
  int devnull;

  if ((devnull = open ("/dev/null", O_RDWR)) == -1)
    return NULL;

  thepid = gpg_invoke_list_keys (pgp, NULL, &fp, NULL, -1, -1, devnull,
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

