/*
 * Copyright (C) 1998 Werner Koch <werner.koch@guug.de>
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "mutt.h"
#include "pgp.h"

static const char *
pkalgbytype(unsigned char type)
{
  switch(type)
  {
    case 1:
    case 2:
    case 3: return "RSA";
    case 16: /* encrypt only */
    case 20: return "ElG";
    case 17: return "DSA";
    default: return "unk";
  }
}

static short canencrypt(unsigned char type)
{
  switch(type)
  {
    case 1:
    case 2:
    case 16:
    case 20:
	return 1;
    default:
	return 0;
  }
}

static short cansign(unsigned char type)
{
  switch(type)
  {
    case 1:
    case 3:
    case 16: /* hmmm: this one can only sign if used in a v3 packet */
    case 17:
    case 20:
	return 1;
    default:
	return 0;
  }
}
/* return values:
 *
 * 1 = sign only
 * 2 = encrypt only
 * 3 = both
 */

static short get_abilities(unsigned char type)
{
  return (canencrypt(type) << 1) | cansign(type);
}

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

static KEYINFO *
parse_pub_line( char *buf, int *is_subkey )
{
    KEYINFO *k=NULL;
    PGPUID *uid = NULL;
    int field = 0;
    char *pend, *p;
    int trust = 0;

    *is_subkey = 0;
    if( !*buf )
	return NULL;
    for( p = buf; p; p = pend ) {
	if( (pend = strchr(p, ':')) )
	    *pend++ = 0;
	field++;
	if( field > 1 && !*p )
	    continue;

	switch( field ) {
	  case 1: /* record type */
	    if( !strcmp(p,"pub") )
		;
	    else if( !strcmp(p,"sub") )
		*is_subkey = 1;
	    else if( !strcmp(p,"sec") )
		;
	    else if( !strcmp(p,"ssb") )
		*is_subkey = 1;
	    else
		return NULL;
	    k = safe_malloc( sizeof(KEYINFO) );
	    memset(k, 0, sizeof(KEYINFO) );
	    break;
	  case 2: /* trust info */
	    switch( *p ) { /* look only at the first letter */
	      case 'e': k->flags |= KEYFLAG_EXPIRED;   break;
	      case 'n': trust = 1;  break;
	      case 'm': trust = 2;  break;
	      case 'f': trust = 3;  break;
	      case 'u': trust = 3;  break;
	      case 'r': k->flags |= KEYFLAG_REVOKED;   break;
	    }
	    break;
	  case 3: /* key length  */
	    k->keylen = atoi(p); /* fixme: add validation checks */
	    break;
	  case 4: /* pubkey algo */
	    k->algorithm = pkalgbytype(atoi(p));
	    k->flags |= get_abilities(atoi(p));
	    break;
	  case 5: /* 16 hex digits with the long keyid. */
	    /* We really should do a check here */
	    k->keyid = safe_strdup(p);
	    break;
	  case 6: /* timestamp (1998-02-28) */
	    break;
	  case 7: /* valid for n days */
	    break;
	  case 8: /* Local id	      */
	    break;
	  case 9: /* ownertrust       */
	    break;
	  case 10: /* name	       */
	    if( !pend || !*p )
		break; /* empty field or no trailing colon */
	    k->address = mutt_new_list();
	    k->address->data = safe_malloc( sizeof(PGPUID) );
	    uid = (PGPUID *)k->address->data;
	    uid->addr = safe_strdup(p);
	    uid->trust = trust;
	    break;
	  case 11: /* signature class  */
	    break;
	  default:
	    break;
	}
    }
    return k;
}

static pid_t gpg_invoke_list_keys(struct pgp_vinfo *pgp,
				  FILE **pgpin, FILE **pgpout, FILE **pgperr,
				  int pgpinfd, int pgpoutfd, int pgperrfd,
				  const char *uids, int secret)
{
  char cmd[HUGE_STRING];
  char tmpcmd[HUGE_STRING];
  char *cp;
  char *keylist;
  
  /* we use gpgm here */
  snprintf(cmd, sizeof(cmd),
	   "%sm --no-verbose --batch --with-colons --list-%skeys ",
	   NONULL(*pgp->binary), secret? "secret-":"");

  keylist = safe_strdup(uids);
  for(cp = strtok(keylist, " "); cp ; cp = strtok(NULL, " "))
  {
    snprintf(tmpcmd, sizeof(tmpcmd), "%s %s",
	     cmd, cp);
    strcpy(cmd, tmpcmd);
  }
  safe_free((void **) &keylist);
  return mutt_create_filter_fd(cmd, pgpin, pgpout, pgperr,
			       pgpinfd, pgpoutfd, pgperrfd);
}

static KEYINFO *
read_ring(struct pgp_vinfo *pgp, int secret )
{
    FILE *fp;
    pid_t thepid;
    char buf[LONG_STRING];
    KEYINFO *db = NULL, **kend, *k = NULL, *kk, *mainkey=NULL;
    int is_sub;

    thepid = gpg_invoke_list_keys(pgp, NULL, &fp, NULL, -1, -1, -1,
							NULL, secret);
    if( thepid == -1 )
	return NULL;

    kend = &db;
    k = NULL;
    while( fgets( buf, sizeof(buf)-1, fp ) ) {
	kk = parse_pub_line(buf, &is_sub );
	if( !kk )
	    continue;
	if( k )
	    kend = &k->next;
	*kend = k = kk;

	if( is_sub ) {
	    k->flags |= KEYFLAG_SUBKEY;
	    k->mainkey = mainkey;
    }
	else
	    mainkey = k;
    }
    if( ferror(fp) )
	mutt_perror("fgets");

    fclose( fp );
    mutt_wait_filter( thepid );

    return db;
}


KEYINFO *gpg_read_pubring(struct pgp_vinfo *pgp)
{
    return read_ring( pgp, 0 );
}


KEYINFO *gpg_read_secring(struct pgp_vinfo *pgp)
{
    return read_ring( pgp, 1 );
}
