/*
 * Copyright (C) 1997 Thomas Roessler <roessler@guug.de>
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


pid_t pgp_invoke_decode (FILE **pgpin, FILE **pgpout, FILE **pgperr,
			 int pgpinfd, int pgpoutfd, int pgperrfd,
			 const char *fname, int need_passphrase)
{
  char cmd[HUGE_STRING];
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_DECODE);

  if(!pgp)
  {
    mutt_error("Unknown PGP version.");
    return -1;
  }
  
  switch(pgp->v)
  {
    case PGP_V2:
      snprintf(cmd, sizeof(cmd), "%scat %s%s | "
	       "%s +language=%s +pubring=%s +secring=%s +verbose=0 +batchmode -f",
	       need_passphrase ? "PGPPASSFD=0; export PGPPASSFD; " : "",
	       need_passphrase ? "- " : "",
	       fname,
	       NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring));
      break;

    case PGP_V3:
      snprintf(cmd, sizeof(cmd), "%scat %s%s | "
	       "%sv +language=%s +pubring=%s +secring=%s +verbose=0 +batchmode -f "
	       "--OutputInformationFD=2",
	       need_passphrase ? "PGPPASSFD=0; export PGPPASSFD; " : "",
	       need_passphrase ? "- " : "",
	       fname,
	       NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring));
      break;
    
    default:
      mutt_error("Unknown PGP version.");
      return -1;
  }
  
  return mutt_create_filter_fd(cmd, pgpin, pgpout, pgperr,
			       pgpinfd, pgpoutfd, pgperrfd);
}


pid_t pgp_invoke_verify(FILE **pgpin, FILE **pgpout, FILE **pgperr,
			int pgpinfd, int pgpoutfd, int pgperrfd,
			const char *signedstuff, const char *sigfile)
{
  char cmd[HUGE_STRING];
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_VERIFY);

  if(!pgp)
  {
    mutt_error("Unknown PGP version.");
    return -1;
  }
  
  switch(pgp->v)
  {
    case PGP_V2:
      snprintf(cmd, sizeof(cmd), 
	       "%s +language=%s +pubring=%s +secring=%s +batchmode +verbose=0 %s %s",
	       NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring), sigfile, signedstuff);
      break;
    
    case PGP_V3:
      snprintf(cmd, sizeof(cmd),
	       "%sv +language=%s +pubring=%s +secring=%s --OutputInformationFD=1 +batchmode +verbose=0 %s %s",
	       NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring), sigfile, signedstuff);
      break;

    default:
      mutt_error("Unknown PGP version.");
      return -1;
  }
  
  return mutt_create_filter_fd(cmd, pgpin, pgpout, pgperr,
			       pgpinfd, pgpoutfd, pgperrfd);
}



pid_t pgp_invoke_decrypt(FILE **pgpin, FILE **pgpout, FILE **pgperr,
			 int pgpinfd, int pgpoutfd, int pgperrfd,
			 const char *fname)
{
  char cmd[HUGE_STRING];
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_DECRYPT);

  if(!pgp)
  {
    mutt_error("Unknown PGP version.");
    return -1;
  }

  switch(pgp->v)
  {
    case PGP_V2:
      snprintf(cmd, sizeof(cmd),
	       "PGPPASSFD=0; export PGPPASSFD; cat - %s | %s +language=%s +pubring=%s +secring=%s "
	       "+verbose=0 +batchmode -f",
	       fname, NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring));
      break;

    case PGP_V3:
      snprintf(cmd, sizeof(cmd),
	       "PGPPASSFD=0; export PGPPASSFD; cat - %s | %sv +language=%s +pubring=%s +secring=%s "
	       "+verbose=0 +batchmode -f --OutputInformationFD=2",
	       fname, NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring));
      break;

    default:
      mutt_error("Unknown PGP version.");
      return -1;

  }

  return mutt_create_filter_fd(cmd, pgpin, pgpout, pgperr,
			    pgpinfd, pgpoutfd, pgperrfd);
}


pid_t pgp_invoke_sign(FILE **pgpin, FILE **pgpout, FILE **pgperr,
		      int pgpinfd, int pgpoutfd, int pgperrfd, 
		      const char *fname)
{
  char cmd[HUGE_STRING];
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_SIGN);

  if(!pgp)
  {
    mutt_error("Unknown PGP version.");
    return -1;
  }

  switch(pgp->v)
  {
    case PGP_V2:
      snprintf(cmd, sizeof(cmd),
	       "PGPPASSFD=0; export PGPPASSFD; cat - %s | %s "
	       "+language=%s +pubring=%s +secring=%s +verbose=0 +batchmode -abfst %s %s",
	       fname, NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring), 
	       PgpSignAs ? "-u" : "",
	       PgpSignAs ? PgpSignAs : "");
      break;

    case PGP_V3:
      snprintf(cmd, sizeof(cmd),
	       "PGPPASSFD=0; export PGPPASSFD; cat - %s | %ss "
	       "+language=%s +pubring=%s +secring=%s +verbose=0 -abft %s %s",
	       fname, NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring),
	       PgpSignAs ? "-u" : "",
	       PgpSignAs ? PgpSignAs : "");
      break;

    default:
      mutt_error("Unknown PGP version.");
      return -1;

  }
  
  return mutt_create_filter_fd(cmd, pgpin, pgpout, pgperr,
			       pgpinfd, pgpoutfd, pgperrfd);
}


pid_t pgp_invoke_encrypt(FILE **pgpin, FILE **pgpout, FILE **pgperr,
			 int pgpinfd, int pgpoutfd, int pgperrfd,
			 const char *fname, const char *uids, int sign)
{
  char cmd[HUGE_STRING];
  char tmpcmd[HUGE_STRING];
  char *cp;
  char *keylist;
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_ENCRYPT);

  if(!pgp)
  {
    mutt_error("Unknown PGP version.");
    return -1;
  }

  switch(pgp->v)
  {
    case PGP_V2:
      snprintf(cmd, sizeof(cmd),
	       "%scat %s%s | %s +language=%s +pubring=%s +secring=%s +verbose=0 %s +batchmode -aeft%s %s%s %s",
	       sign ? "PGPPASSFD=0; export PGPPASSFD; " : "",
	       sign ? "- " : "",
	       fname,
	       NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring), 
	       option(OPTPGPENCRYPTSELF) ? "+encrypttoself" : "",
	       sign ? "s" : "",
	       sign && PgpSignAs ? "-u " : "",
	       sign && PgpSignAs ? PgpSignAs : "",
	       uids);
      break;

    case PGP_V3:
      snprintf(cmd, sizeof(cmd),
	       "%scat %s%s | %se +language=%s +pubring=%s +secring=%s +verbose=0 %s +batchmode +nobatchinvalidkeys=off -aft%s %s%s",
	       sign ? "PGPPASSFD=0; export PGPPASSFD; " : "",
	       sign ? "- " : "",
	       fname,
	       NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring), 
	       option(OPTPGPENCRYPTSELF) ? "+encrypttoself" : "",
	       sign ? "s" : "",
	       sign && PgpSignAs ? "-u " : "",
	       sign && PgpSignAs ? PgpSignAs : "");

      keylist = safe_strdup(uids);
      for(cp = strtok(keylist, " "); cp ; cp = strtok(NULL, " "))
      {
	snprintf(tmpcmd, sizeof(tmpcmd), "%s -r %s", 
		 cmd, cp);
	strcpy(cmd, tmpcmd);
      }
      safe_free((void **) &keylist);
      break;

    default:
      mutt_error("Unknown PGP version.");
      return -1;

  }
  
  return mutt_create_filter_fd(cmd, pgpin, pgpout, pgperr, 
			       pgpinfd, pgpoutfd, pgperrfd);
}


void pgp_invoke_extract(const char *fname)
{
  char cmd[HUGE_STRING];
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_EXTRACT);

  if(!pgp)
  {
    mutt_error("Unknown PGP version.");
    return;
  }

  switch(pgp->v)
  {
    case PGP_V2:
      snprintf(cmd, sizeof(cmd), "%s +language=%s +pubring=%s +secring=%s -ka %s",
	       NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring), fname);
      break;

    case PGP_V3:
      snprintf(cmd, sizeof(cmd), "%sk +language=%s +pubring=%s +secring=%s -a --OutputInformationFD=1 %s",
	       NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring), fname);
      break;

    default:
      mutt_error("Unknown PGP version.");
      return;

  }
  mutt_system(cmd);
}


pid_t pgp_invoke_verify_key(FILE **pgpin, FILE **pgpout, FILE **pgperr,
			    int pgpinfd, int pgpoutfd, int pgperrfd, const char *id)
{
  char cmd[HUGE_STRING];
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_VERIFY_KEY);

  if(!pgp)
  {
    mutt_error("Unknown PGP version.");
    return -1;
  }

  switch(pgp->v)
  {
    case PGP_V2:
      snprintf(cmd, sizeof(cmd), "%s +language=%s +pubring=%s +secring=%s +batchmode -kcc 0x%8s",
	       NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring), id);
      break;

    case PGP_V3:
      snprintf(cmd, sizeof(cmd), "%sk +language=%s +pubring=%s +secring=%s +batchmode -c --OutputInformationFD=1 0x%8s",
	       NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring), id);
      break;

    default:
          mutt_error("Unknown PGP version.");
      return -1;
    
  }
  
  return mutt_create_filter_fd(cmd, pgpin, pgpout, pgperr,
			       pgpinfd, pgpoutfd, pgperrfd);
}


pid_t pgp_invoke_extract_key(FILE **pgpin, FILE **pgpout, FILE **pgperr,
			     int pgpinfd, int pgpoutfd, int pgperrfd, const char *id)
{
  char cmd[HUGE_STRING];
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_EXTRACT_KEY);

  if(!pgp)
  {
    mutt_error("Unknown PGP version.");
    return -1;
  }

  switch(pgp->v)
  {
    case PGP_V2:
      snprintf(cmd, sizeof(cmd), "%s -kxaf +language=%s +pubring=%s +secring=%s 0x%8s",
	       NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring), id);
      break;

    case PGP_V3:
      snprintf(cmd, sizeof(cmd), "%sk -xa +language=%s +pubring=%s +secring=%s --OutputInformationFD=1 0x%8s",
	       NONULL (*pgp->binary), NONULL (*pgp->language), NONULL (*pgp->pubring), NONULL (*pgp->secring), id);
      break;

    default:
      mutt_error("Unknown PGP version.");
      return -1;

  }
  
  return mutt_create_filter_fd(cmd, pgpin, pgpout, pgperr,
			       pgpinfd, pgpoutfd, pgperrfd);
}
