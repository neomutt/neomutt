/*
 * Copyright (C) 1996,1997 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (c) 1998,1999 Thomas Roessler <roessler@guug.de>
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

/*
 * This file contains all of the PGP routines necessary to sign, encrypt,
 * verify and decrypt PGP messages in either the new PGP/MIME format, or
 * in the older Application/Pgp format.  It also contains some code to
 * cache the user's passphrase for repeat use when decrypting or signing
 * a message.
 */

#include "mutt.h"
#include "mutt_curses.h"
#include "pgp.h"
#include "mime.h"
#include "parse.h"

#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#ifdef _PGPPATH


char PgpPass[STRING];
static time_t PgpExptime = 0; /* when does the cached passphrase expire? */

static struct pgp_vinfo pgp_vinfo[] =
{

  { PGP_V2, 	
    "pgp2",	
    &PgpV2,	&PgpV2Pubring,	&PgpV2Secring, 	&PgpV2Language, 
    pgp_get_candidates,
    pgp_v2_invoke_decode, pgp_v2_invoke_verify, pgp_v2_invoke_decrypt,
    pgp_v2_invoke_sign, pgp_v2_invoke_encrypt, pgp_v2_invoke_import,
    pgp_v2_invoke_export, pgp_v2_invoke_verify_key 
  },

  { PGP_V3, 	
    "pgp3",	
    &PgpV3,	&PgpV3Pubring,	&PgpV3Secring, 	&PgpV3Language, 
    pgp_get_candidates,
    pgp_v3_invoke_decode, pgp_v3_invoke_verify, pgp_v3_invoke_decrypt,
    pgp_v3_invoke_sign, pgp_v3_invoke_encrypt, pgp_v3_invoke_import,
    pgp_v3_invoke_export, pgp_v3_invoke_verify_key 
  },
  
  { PGP_V3, 	
    "pgp5",	
    &PgpV3,	&PgpV3Pubring,	&PgpV3Secring, 	&PgpV3Language, 
    pgp_get_candidates,
    pgp_v3_invoke_decode, pgp_v3_invoke_verify, pgp_v3_invoke_decrypt,
    pgp_v3_invoke_sign, pgp_v3_invoke_encrypt, pgp_v3_invoke_import,
    pgp_v3_invoke_export, pgp_v3_invoke_verify_key 
  },
  
  { PGP_GPG, 	
    "gpg",	
    &PgpGpg,	&PgpGpgDummy,	&PgpGpgDummy, &PgpGpgDummy,
    gpg_get_candidates,
    pgp_gpg_invoke_decode, pgp_gpg_invoke_verify, pgp_gpg_invoke_decrypt,
    pgp_gpg_invoke_sign, pgp_gpg_invoke_encrypt, pgp_gpg_invoke_import,
    pgp_gpg_invoke_export, pgp_gpg_invoke_verify_key 
  },
  
  { PGP_UNKNOWN,
    NULL, 
    NULL, NULL, NULL, NULL,
    NULL,
    NULL, NULL, NULL,
    NULL, NULL, NULL, 
    NULL, NULL
  }
};

static struct
{
  enum pgp_ops op;
  char **str;
} 
pgp_opvers[] =
{
  { PGP_DECODE, 	&PgpReceiveVersion },
  { PGP_VERIFY, 	&PgpReceiveVersion },
  { PGP_DECRYPT,	&PgpReceiveVersion },
  { PGP_SIGN,		&PgpSendVersion    },
  { PGP_ENCRYPT,	&PgpSendVersion	  },
  { PGP_VERIFY_KEY,	&PgpSendVersion	  },
  { PGP_IMPORT,		&PgpKeyVersion	  },
  { PGP_EXPORT,		&PgpKeyVersion	  },
  { PGP_LAST_OP,	NULL		  }
};



void pgp_void_passphrase (void)
{
  memset (PgpPass, 0, sizeof (PgpPass));
  PgpExptime = 0;
}

int pgp_valid_passphrase (void)
{
  time_t now = time (NULL);

  if (now < PgpExptime) return 1; /* just use the cached copy. */
  pgp_void_passphrase ();

  if (mutt_get_password (_("Enter PGP passphrase:"), PgpPass, sizeof (PgpPass)) == 0)
  {
    PgpExptime = time (NULL) + PgpTimeout;
    return (1);
  }
  else
  {
    PgpExptime = 0;
    return (0);
  }
  /* not reached */
}

void mutt_forget_passphrase (void)
{
  pgp_void_passphrase ();
  mutt_message _("PGP passphrase forgotten.");
}


struct pgp_vinfo *pgp_get_vinfo(enum pgp_ops op)
{
  int i;
  char *version = "default";
  char msg[LONG_STRING];
  
  for(i = 0; pgp_opvers[i].op != PGP_LAST_OP; i++)
  {
    if(pgp_opvers[i].op == op)
    {
      version = *pgp_opvers[i].str;
      break;
    }
  }
  
  if (!mutt_strcasecmp(version, "default"))
    version = PgpDefaultVersion;
  
  for(i = 0; pgp_vinfo[i].name; i++)
  {
    if(!mutt_strcasecmp(pgp_vinfo[i].name, version))
      return &pgp_vinfo[i];
  }

  snprintf(msg, sizeof(msg), _("Unknown PGP version \"%s\"."),
	   version);
  mutt_error(msg);

  return NULL;
}

char *pgp_keyid(pgp_key_t *k)
{
  if((k->flags & KEYFLAG_SUBKEY) && k->parent)
    k = k->parent;

  return _pgp_keyid(k);
}

char *_pgp_keyid(pgp_key_t *k)
{
  if(option(OPTPGPLONGIDS))
    return k->keyid;
  else
    return (k->keyid + 8);
}

/* ----------------------------------------------------------------------------
 * Routines for handing PGP input.
 */

/* print the current time to avoid spoofing of the signature output */
static void pgp_current_time (STATE *s)
{
  time_t t;
  char p[STRING];

  state_puts (_("[-- PGP output follows (current time: "), s);

  t = time (NULL);
  strfcpy (p, asctime (localtime (&t)), sizeof (p));
  p[mutt_strlen (p) - 1] = 0; /* kill the newline */
  state_puts (p, s);

  state_puts (") --]\n", s);
}


/* Support for the Application/PGP Content Type. */

void pgp_application_pgp_handler (BODY *m, STATE *s)
{
  int needpass = -1, pgp_keyblock = 0;
  int clearsign = 0;
  long start_pos = 0;
  long bytes, last_pos, offset;
  char buf[HUGE_STRING];
  char outfile[_POSIX_PATH_MAX];
  char tmpfname[_POSIX_PATH_MAX];
  FILE *pgpout = NULL, *pgpin, *pgperr;
  FILE *tmpfp;
  pid_t thepid;
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_DECODE);

  if(!pgp)
    return;
  
  fseek (s->fpin, m->offset, 0);
  last_pos = m->offset;
  
  for (bytes = m->length; bytes > 0;)
  {
    if (fgets (buf, sizeof (buf) - 1, s->fpin) == NULL)
      break;
    offset = ftell (s->fpin);
    bytes -= (offset - last_pos); /* don't rely on mutt_strlen(buf) */
    last_pos = offset;

    if (mutt_strncmp ("-----BEGIN PGP ", buf, 15) == 0)
    {
      clearsign = 0;
      start_pos = last_pos;

      if (mutt_strcmp ("MESSAGE-----\n", buf + 15) == 0)
        needpass = 1;
      else if (mutt_strcmp ("SIGNED MESSAGE-----\n", buf + 15) == 0)
      {
	clearsign = 1;
        needpass = 0;
      }
      else if (!option(OPTDONTHANDLEPGPKEYS) &&
	       mutt_strcmp ("PUBLIC KEY BLOCK-----\n", buf + 15) == 0) 
      {
        needpass = 0;
        pgp_keyblock =1;
      } 
      else
      {
	if (s->prefix)
	  state_puts (s->prefix, s);
	state_puts (buf, s);
	continue;
      }

      if(!clearsign || s->flags & M_VERIFY)
      {

	/* invoke PGP */
	
	mutt_mktemp (outfile);
	if ((pgpout = safe_fopen (outfile, "w+")) == NULL)
	{
	  mutt_perror (outfile);
	  return;
	}
	
	mutt_mktemp (tmpfname);
	if ((tmpfp = safe_fopen(tmpfname, "w+")) == NULL)
	{
	  mutt_perror(tmpfname);
	  fclose(pgpout); pgpout = NULL;
	  return;
	}
	
	fputs (buf, tmpfp);
	while (bytes > 0 && fgets (buf, sizeof (buf) - 1, s->fpin) != NULL)
	{
	  offset = ftell (s->fpin);
	  bytes -= (offset - last_pos); /* don't rely on mutt_strlen(buf) */
	  last_pos = offset;
	  
	  fputs (buf, tmpfp);
	  if ((needpass && mutt_strcmp ("-----END PGP MESSAGE-----\n", buf) == 0) ||
	      (!needpass 
             && (mutt_strcmp ("-----END PGP SIGNATURE-----\n", buf) == 0
                 || mutt_strcmp ("-----END PGP PUBLIC KEY BLOCK-----\n",buf) == 0)))
	    break;
	}

	fclose(tmpfp);
	
	if ((thepid = pgp->invoke_decode (pgp,
					  &pgpin, NULL, 
					  &pgperr, -1,
					  fileno (pgpout), 
					  -1, tmpfname, 
					  needpass)) == -1)
	{
	  fclose (pgpout); pgpout = NULL;
	  mutt_unlink(tmpfname);
	  state_puts (_("[-- Error: unable to create PGP subprocess! --]\n"), s);
	  state_puts (buf, s);
	  continue;
	}
	
	if (needpass)
	{
	if (!pgp_valid_passphrase ())
	    pgp_void_passphrase ();
	  fputs (PgpPass, pgpin);
	  fputc ('\n', pgpin);
	}

	fclose (pgpin);
	
	if (s->flags & M_DISPLAY)
	  pgp_current_time (s);
	
	mutt_wait_filter (thepid);

	mutt_unlink(tmpfname);
	
	if (s->flags & M_DISPLAY)
	  mutt_copy_stream(pgperr, s->fpout);
	fclose (pgperr);
	
	if (s->flags & M_DISPLAY)
	  state_puts (_("\n[-- End of PGP output --]\n\n"), s);
      }
    
      if(s->flags & M_DISPLAY)
      {
	if (needpass)
	  state_puts (_("[-- BEGIN PGP MESSAGE --]\n\n"), s);
	else if (pgp_keyblock)
	  state_puts (_("[-- BEGIN PGP PUBLIC KEY BLOCK --]\n"), s);
	else
	  state_puts (_("[-- BEGIN PGP SIGNED MESSAGE --]\n\n"), s);
      }

      /* Use PGP's output if there was no clearsig signature. */
      
      if(!clearsign)
      {
	fflush (pgpout);
	rewind (pgpout);
	while (fgets (buf, sizeof (buf) - 1, pgpout) != NULL)
	{
	  if (s->prefix)
	    state_puts (s->prefix, s);
	  state_puts (buf, s);
	}
      }

      /* Close the temporary files iff they were created. 
       * The condition used to be !clearsign || s->flags & M_VERIFY,
       * but gcc would complain then.
       */
      
      if(pgpout)
      {
	fclose (pgpout);
	pgpout = NULL;
	mutt_unlink(outfile);
      }

      /* decode clearsign stuff */
      
      if(clearsign)
      {

	/* rationale: We want PGP's error messages, but in the times
	 * of PGP 5.0 we can't rely on PGP to do the dash
	 * escape decoding - so we have to do this
	 * ourselves.
	 */
	
	int armor_header = 1;
	int complete = 1;
	
	fseek(s->fpin, start_pos, SEEK_SET);
	bytes += (last_pos - start_pos);
	last_pos = start_pos;
	offset = start_pos;
	while(bytes > 0 && fgets(buf, sizeof(buf) - 1, s->fpin) != NULL)
	{
	  offset = ftell(s->fpin);
	  bytes -= (offset - last_pos);
	  last_pos = offset;

	  if(complete)
	  {
	    if (!mutt_strcmp(buf, "-----BEGIN PGP SIGNATURE-----\n"))
	      break;
	    
	    if(armor_header)
	    {
	      if(*buf == '\n')
		armor_header = 0;
	    }
	    else
	    {
	      if(s->prefix)
		state_puts(s->prefix, s);
	      
	      if(buf[0] == '-' && buf [1] == ' ')
		state_puts(buf + 2, s);
	      else
		state_puts(buf, s);
	    }
	  } 
	  else 
	  {
	    if(!armor_header)
	      state_puts(buf, s);
	  }
	  
	  complete = strchr(buf, '\n') != NULL;
	}
	
	if (complete && !mutt_strcmp(buf, "-----BEGIN PGP SIGNATURE-----\n"))
	{
	  while(bytes > 0 && fgets(buf, sizeof(buf) - 1, s->fpin) != NULL)
	  {
	    offset = ftell(s->fpin);
	    bytes -= (offset - last_pos);
	    last_pos = offset;
	    
	    if(complete && !mutt_strcmp(buf, "-----END PGP SIGNATURE-----\n"))
	      break;
	    
	    complete = strchr(buf, '\n') != NULL;
	  }
	}
      }
      
      if (s->flags & M_DISPLAY)
      {
	if (needpass)
	  state_puts (_("\n[-- END PGP MESSAGE --]\n"), s);
	else if (pgp_keyblock)
	  state_puts (_("[-- END PGP PUBLIC KEY BLOCK --]\n"), s);
	else
	  state_puts (_("\n[-- END PGP SIGNED MESSAGE --]\n"), s);
      }
    }
    else
    {
      if (s->prefix)
	state_puts (s->prefix, s);
      state_puts (buf, s);
    }
  }

  if (needpass == -1)
  {
    state_puts (_("[-- Error: could not find beginning of PGP message! --]\n\n"), s);
    return;
  }

}

int mutt_is_multipart_signed (BODY *b)
{
  char *p;

  if (!b || b->type != TYPEMULTIPART ||
      !b->subtype || mutt_strcasecmp (b->subtype, "signed") ||
      !(p = mutt_get_parameter ("protocol", b->parameter)) ||
      (mutt_strcasecmp (p, "application/pgp-signature")
      && mutt_strcasecmp (p, "multipart/mixed")))
    return 0;

  return PGPSIGN;
}
   
     
int mutt_is_multipart_encrypted (BODY *b)
{
  char *p;
  
  if (!b || b->type != TYPEMULTIPART ||
      !b->subtype || mutt_strcasecmp (b->subtype, "encrypted") ||
      !(p = mutt_get_parameter ("protocol", b->parameter)) ||
      mutt_strcasecmp (p, "application/pgp-encrypted"))
    return 0;
  
  return PGPENCRYPT;
}

int mutt_is_application_pgp (BODY *m)
{
  int t = 0;
  char *p;
  
  if (m->type == TYPEAPPLICATION)
  {
    if (!mutt_strcasecmp (m->subtype, "pgp") || !mutt_strcasecmp (m->subtype, "x-pgp-message"))
    {
      if ((p = mutt_get_parameter ("x-action", m->parameter))
	  && (!mutt_strcasecmp (p, "sign") || !mutt_strcasecmp (p, "signclear")))
	t |= PGPSIGN;
      else if((p = mutt_get_parameter ("format", m->parameter))
	      && !mutt_strcasecmp(p, "keys-only"))
	t |= PGPKEY;

      if ((p = mutt_get_parameter ("format", m->parameter)) && 
	  !mutt_strcasecmp (p, "keys-only"))
	t |= PGPKEY;

      if(!t) t |= PGPENCRYPT;  /* not necessarily correct, but... */
    }

    if (!mutt_strcasecmp (m->subtype, "pgp-signed"))
      t |= PGPSIGN;

    if (!mutt_strcasecmp (m->subtype, "pgp-keys"))
      t |= PGPKEY;
  }
  return t;
}

int pgp_query (BODY *m)
{
  int t = 0;

  t |= mutt_is_application_pgp(m);
  
  /* Check for PGP/MIME messages. */
  if (m->type == TYPEMULTIPART)
  {
    if(mutt_is_multipart_signed(m))
      t |= PGPSIGN;
    else if (mutt_is_multipart_encrypted(m))
      t |= PGPENCRYPT;
  }

  if(m->type == TYPEMULTIPART || m->type == TYPEMESSAGE)
  {
    BODY *p;
 
    for(p = m->parts; p; p = p->next)
      t |= pgp_query(p);
  }
  
  return t;
}

static void pgp_fetch_signatures (BODY ***signatures, BODY *a, int *n)
{
  for (; a; a = a->next)
  {
    if(a->type == TYPEMULTIPART)
      pgp_fetch_signatures (signatures, a->parts, n);
    else
    {
      if((*n % 5) == 0)
	safe_realloc((void **) signatures, (*n + 6) * sizeof(BODY **));

      (*signatures)[(*n)++] = a;
    }
  }
}

static int pgp_write_signed(BODY *a, STATE *s, const char *tempfile)
{
  FILE *fp;
  int c;
  short hadcr;
  size_t bytes;

  if(!(fp = safe_fopen (tempfile, "w")))
  {
    mutt_perror(tempfile);
    return -1;
  }
      
  fseek (s->fpin, a->hdr_offset, 0);
  bytes = a->length + a->offset - a->hdr_offset;
  hadcr = 0;
  while (bytes > 0)
  {
    if((c = fgetc(s->fpin)) == EOF)
      break;
    
    bytes--;
    
    if(c == '\r')
      hadcr = 1;
    else 
    {
      if(c == '\n' && !hadcr)
	fputc('\r', fp);
      
      hadcr = 0;
    }
    
    fputc(c, fp);
    
  }
  fclose (fp);

  return 0;
}

static int pgp_verify_one (BODY *sigbdy, STATE *s, const char *tempfile, struct pgp_vinfo *pgp)
{
  char sigfile[_POSIX_PATH_MAX], pgperrfile[_POSIX_PATH_MAX];
  FILE *fp, *pgpout, *pgperr;
  pid_t thepid;
  
  snprintf (sigfile, sizeof (sigfile), "%s.asc", tempfile);
  
  if(!(fp = safe_fopen (sigfile, "w")))
  {
    mutt_perror(sigfile);
    return -1;
  }
	
  fseek (s->fpin, sigbdy->offset, 0);
  mutt_copy_bytes (s->fpin, fp, sigbdy->length);
  fclose (fp);
  
  mutt_mktemp(pgperrfile);
  if(!(pgperr = safe_fopen(pgperrfile, "w+")))
  {
    mutt_perror(pgperrfile);
    unlink(sigfile);
    return -1;
  }
  
  pgp_current_time (s);
  
  if((thepid = pgp->invoke_verify (pgp,
				   NULL, &pgpout, NULL, 
				   -1, -1, fileno(pgperr),
				   tempfile, sigfile)) != -1)
  {
    mutt_copy_stream(pgpout, s->fpout);
    fclose (pgpout);
    fflush(pgperr);
    rewind(pgperr);
    mutt_copy_stream(pgperr, s->fpout);
    fclose(pgperr);
    
    mutt_wait_filter (thepid);
  }
  
  state_puts (_("[-- End of PGP output --]\n\n"), s);
  
  mutt_unlink (sigfile);
  mutt_unlink (pgperrfile);

  return 0;
}

/*
 * This routine verifies a PGP/MIME signed body.
 */
void pgp_signed_handler (BODY *a, STATE *s)
{
  char tempfile[_POSIX_PATH_MAX];
  char *protocol;
  int protocol_major = TYPEOTHER;
  char *protocol_minor = NULL;
  
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_VERIFY);

  BODY **signatures = NULL;
  int sigcnt = 0;
  int i;

  
  if (!pgp)
    return;

  protocol = mutt_get_parameter ("protocol", a->parameter);
  a = a->parts;

  /* extract the protocol information */
  
  if (protocol)
  {
    char major[STRING];
    char *t;

    if ((protocol_minor = strchr(protocol, '/'))) protocol_minor++;
    
    strfcpy(major, protocol, sizeof(major));
    if((t = strchr(major, '/')))
      *t = '\0';
    
    protocol_major = mutt_check_mime_type (major);
  }

  /* consistency check */

  if (!(a && a->next && a->next->type == protocol_major && 
      !mutt_strcasecmp(a->next->subtype, protocol_minor)))
  {
    state_puts(_("[-- Error: Inconsistant multipart/signed structure! --]\n\n"), s);
    mutt_body_handler (a, s);
    return;
  }

  if(!(protocol_major == TYPEAPPLICATION && !mutt_strcasecmp(protocol_minor, "pgp-signature"))
     && !(protocol_major == TYPEMULTIPART && !mutt_strcasecmp(protocol_minor, "mixed")))
  {
    state_printf(s, _("[-- Error: Unknown multipart/signed protocol %s! --]\n\n"), protocol);
    mutt_body_handler (a, s);
    return;
  }
  
  if (s->flags & M_DISPLAY)
  {
    
    pgp_fetch_signatures(&signatures, a->next, &sigcnt);
    
    if (sigcnt)
    {
      mutt_mktemp (tempfile);
      if (pgp_write_signed (a, s, tempfile) == 0)
      {
	for (i = 0; i < sigcnt; i++)
	{
	  if (signatures[i]->type == TYPEAPPLICATION 
	      && !mutt_strcasecmp(signatures[i]->subtype, "pgp-signature"))
	    pgp_verify_one (signatures[i], s, tempfile, pgp);
	  else
	    state_printf (s, _("[-- Warning: We can't verify %s/%s signatures. --]\n\n"),
			  TYPE(signatures[i]), signatures[i]->subtype);
	}
      }
      
      mutt_unlink (tempfile);
      
      /* Now display the signed body */
      state_puts (_("[-- The following data is signed --]\n\n"), s);


      safe_free((void **) &signatures);
    }
    else
      state_puts (_("[-- Warning: Can't find any signatures. --]\n\n"), s);
  }
  
  mutt_body_handler (a, s);
  
  if (s->flags & M_DISPLAY && sigcnt)
    state_puts (_("\n[-- End of signed data --]\n"), s);
}

/* Extract pgp public keys from messages or attachments */

void pgp_extract_keys_from_messages (HEADER *h)
{
  int i;
  STATE s;
  char tempfname[_POSIX_PATH_MAX];
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_IMPORT);
  
  if(!pgp)
    return;
  
  if(h)
  {
    mutt_parse_mime_message(Context, h);
    if(h->pgp & PGPENCRYPT && !pgp_valid_passphrase())
      return;
  }

  memset(&s, 0, sizeof(STATE));
  
  mutt_mktemp(tempfname);
  if(!(s.fpout = safe_fopen(tempfname, "w")))
  {
    mutt_perror(tempfname);
    return;
  }

  set_option(OPTDONTHANDLEPGPKEYS);
  
  if(!h)
  {
    for(i = 0; i < Context->vcount; i++)
    {
      if(Context->hdrs[Context->v2r[i]]->tagged)
      {
	mutt_parse_mime_message(Context, Context->hdrs[Context->v2r[i]]);
	if(Context->hdrs[Context->v2r[i]]->pgp & PGPENCRYPT
	   && !pgp_valid_passphrase())
	{
	  fclose(s.fpout);
	  goto bailout;
	}
	mutt_pipe_message_to_state(Context->hdrs[Context->v2r[i]], &s);
      }
    }
  } 
  else
  {
    mutt_parse_mime_message(Context, h);
    if(h->pgp & PGPENCRYPT && !pgp_valid_passphrase())
    {
      fclose(s.fpout);
      goto bailout;
    }
    mutt_pipe_message_to_state(h, &s);
  }
      
  fclose(s.fpout);
  endwin();
  pgp->invoke_import(pgp, tempfname);
  mutt_any_key_to_continue(NULL);

  bailout:
  
  mutt_unlink(tempfname);
  unset_option(OPTDONTHANDLEPGPKEYS);
  
}

static void pgp_extract_keys_from_attachment(struct pgp_vinfo *pgp,
					     FILE *fp, BODY *top)
{
  STATE s;
  FILE *tempfp;
  char tempfname[_POSIX_PATH_MAX];

  mutt_mktemp(tempfname);
  if(!(tempfp = safe_fopen(tempfname, "w")))
  {
    mutt_perror(tempfname);
    return;
  }

  memset(&s, 0, sizeof(STATE));
  
  s.fpin = fp;
  s.fpout = tempfp;
  
  mutt_body_handler(top, &s);

  fclose(tempfp);

  pgp->invoke_import(pgp, tempfname);
  mutt_any_key_to_continue(NULL);

  mutt_unlink(tempfname);

}

void pgp_extract_keys_from_attachment_list (FILE *fp, int tag, BODY *top)
{
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_IMPORT);
  
  if(!pgp)
    return;
  
  if(!fp)
  {
    mutt_error _("Internal error. Inform <roessler@guug.de>.");
    return;
  }

  endwin();
  set_option(OPTDONTHANDLEPGPKEYS);
  
  for(; top; top = top->next)
  {
    if(!tag || top->tagged)
      pgp_extract_keys_from_attachment (pgp, fp, top);
    
    if(!tag)
      break;
  }
  
  unset_option(OPTDONTHANDLEPGPKEYS);
}

BODY *pgp_decrypt_part (BODY *a, STATE *s, FILE *fpout)
{
  char buf[LONG_STRING];
  FILE *pgpin, *pgpout, *pgperr, *pgptmp;
  struct stat info;
  BODY *tattach;
  int len;
  char pgperrfile[_POSIX_PATH_MAX];
  char pgptmpfile[_POSIX_PATH_MAX];
  pid_t thepid;
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_DECRYPT);
  
  if(!pgp)
    return NULL;
  
  mutt_mktemp (pgperrfile);
  if ((pgperr = safe_fopen (pgperrfile, "w+")) == NULL)
  {
    mutt_perror (pgperrfile);
    return NULL;
  }
  unlink (pgperrfile);

  mutt_mktemp (pgptmpfile);
  if((pgptmp = safe_fopen (pgptmpfile, "w")) == NULL)
  {
    mutt_perror (pgptmpfile);
    fclose(pgperr);
    return NULL;
  }

  /* Position the stream at the beginning of the body, and send the data to
   * the temporary file.
   */

  fseek (s->fpin, a->offset, 0);
  mutt_copy_bytes (s->fpin, pgptmp, a->length);
  fclose (pgptmp);

  if ((thepid = pgp->invoke_decrypt (pgp, &pgpin, &pgpout, NULL, -1, -1,
				    fileno (pgperr), pgptmpfile)) == -1)
  {
    fclose (pgperr);
    unlink (pgptmpfile);
    if (s->flags & M_DISPLAY)
      state_puts (_("[-- Error: could not create a PGP subprocess! --]\n\n"), s);
    return (NULL);
  }

  /* send the PGP passphrase to the subprocess */
  fputs (PgpPass, pgpin);
  fputc ('\n', pgpin);
  fclose(pgpin);
  
  /* Read the output from PGP, and make sure to change CRLF to LF, otherwise
   * read_mime_header has a hard time parsing the message.
   */
  while (fgets (buf, sizeof (buf) - 1, pgpout) != NULL)
  {
    len = mutt_strlen (buf);
    if (len > 1 && buf[len - 2] == '\r')
      strcpy (buf + len - 2, "\n");
    fputs (buf, fpout);
  }

  fclose (pgpout);
  mutt_wait_filter (thepid);
  mutt_unlink(pgptmpfile);
  
  if (s->flags & M_DISPLAY)
  {
    fflush (pgperr);
    rewind (pgperr);
    mutt_copy_stream (pgperr, s->fpout);
    state_puts (_("[-- End of PGP output --]\n\n"), s);
  }
  fclose (pgperr);

  fflush (fpout);
  rewind (fpout);
  if ((tattach = mutt_read_mime_header (fpout, 0)) != NULL)
  {
    /*
     * Need to set the length of this body part.
     */
    fstat (fileno (fpout), &info);
    tattach->length = info.st_size - tattach->offset;

    /* See if we need to recurse on this MIME part.  */

    mutt_parse_part (fpout, tattach);
  }

  return (tattach);
}

int pgp_decrypt_mime (FILE *fpin, FILE **fpout, BODY *b, BODY **cur)
{
  char tempfile[_POSIX_PATH_MAX];
  STATE s;
  
  if(!mutt_is_multipart_encrypted(b))
    return -1;

  if(!b->parts || !b->parts->next)
    return -1;
  
  b = b->parts->next;
  
  memset (&s, 0, sizeof (s));
  s.fpin = fpin;
  mutt_mktemp (tempfile);
  if ((*fpout = safe_fopen (tempfile, "w+")) == NULL)
  {
    mutt_perror (tempfile);
    return (-1);
  }
  unlink (tempfile);

  *cur = pgp_decrypt_part (b, &s, *fpout);

  rewind (*fpout);
  return (0);
}

void pgp_encrypted_handler (BODY *a, STATE *s)
{
  char tempfile[_POSIX_PATH_MAX];
  FILE *fpout, *fpin;
  BODY *tattach;

  a = a->parts;
  if (!a || a->type != TYPEAPPLICATION || !a->subtype || 
      mutt_strcasecmp ("pgp-encrypted", a->subtype) != 0 ||
      !a->next || a->next->type != TYPEAPPLICATION || !a->next->subtype ||
      mutt_strcasecmp ("octet-stream", a->next->subtype) != 0)
  {
    if (s->flags & M_DISPLAY)
      state_puts (_("[-- Error: malformed PGP/MIME message! --]\n\n"), s);
    return;
  }

  /*
   * Move forward to the application/pgp-encrypted body.
   */
  a = a->next;

  mutt_mktemp (tempfile);
  if ((fpout = safe_fopen (tempfile, "w+")) == NULL)
  {
    if (s->flags & M_DISPLAY)
      state_puts (_("[-- Error: could not create temporary file! --]\n"), s);
    return;
  }

  if (s->flags & M_DISPLAY) pgp_current_time (s);

  if ((tattach = pgp_decrypt_part (a, s, fpout)) != NULL)
  {
    if (s->flags & M_DISPLAY)
      state_puts (_("[-- The following data is PGP/MIME encrypted --]\n\n"), s);

    fpin = s->fpin;
    s->fpin = fpout;
    mutt_body_handler (tattach, s);
    s->fpin = fpin;

    if (s->flags & M_DISPLAY)
      state_puts (_("\n[-- End of PGP/MIME encrypted data --]\n"), s);

    mutt_free_body (&tattach);
  }

  fclose (fpout);
  mutt_unlink(tempfile);
}

/* ----------------------------------------------------------------------------
 * Routines for sending PGP/MIME messages.
 */

static void convert_to_7bit (BODY *a)
{
  while (a)
  {
    if (a->type == TYPEMULTIPART)
    {
      if (a->encoding != ENC7BIT)
      {
        a->encoding = ENC7BIT;
	convert_to_7bit(a->parts);
      }
      else if (option (OPTPGPSTRICTENC))
	convert_to_7bit (a->parts);
    } 
    else if (a->type == TYPEMESSAGE
	     && mutt_strcasecmp(a->subtype, "delivery-status"))
    {
      if(a->encoding != ENC7BIT)
	mutt_message_to_7bit(a, NULL);
    }
    else if (a->encoding == ENC8BIT)
      a->encoding = ENCQUOTEDPRINTABLE;
    else if (a->encoding == ENCBINARY)
      a->encoding = ENCBASE64;
    else if (a->content && (a->content->from || (a->content->space && option (OPTPGPSTRICTENC))))
      a->encoding = ENCQUOTEDPRINTABLE;
    a = a->next;
  }
}

static BODY *pgp_sign_message (BODY *a)
{
  BODY *t;
  char buffer[LONG_STRING];
  char sigfile[_POSIX_PATH_MAX], signedfile[_POSIX_PATH_MAX];
  FILE *pgpin, *pgpout, *pgperr, *fp, *sfp;
  int err = 0;
  int empty = 1;
  pid_t thepid;
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_SIGN);
  
  if(!pgp)
    return NULL;
  
  convert_to_7bit (a); /* Signed data _must_ be in 7-bit format. */

  mutt_mktemp (sigfile);
  if ((fp = safe_fopen (sigfile, "w")) == NULL)
  {
    return (NULL);
  }

  mutt_mktemp (signedfile);
  if ((sfp = safe_fopen(signedfile, "w")) == NULL)
  {
    mutt_perror(signedfile);
    fclose(fp);
    unlink(sigfile);
    return NULL;
  }
  
  mutt_write_mime_header (a, sfp);
  fputc ('\n', sfp);
  mutt_write_mime_body (a, sfp);
  fclose(sfp);
  
  if((thepid = pgp->invoke_sign(pgp, &pgpin, &pgpout, &pgperr,
			       -1, -1, -1, signedfile)) == -1)
  {
    mutt_perror _("Can't open PGP subprocess!");
    fclose(fp);
    unlink(sigfile);
    unlink(signedfile);
    return NULL;
  }
  
  fputs(PgpPass, pgpin);
  fputc('\n', pgpin);
  fclose(pgpin);
  
  /*
   * Read back the PGP signature.  Also, change MESSAGE=>SIGNATURE as
   * recommended for future releases of PGP.
   */
  while (fgets (buffer, sizeof (buffer) - 1, pgpout) != NULL)
  {
    if (mutt_strcmp ("-----BEGIN PGP MESSAGE-----\n", buffer) == 0)
      fputs ("-----BEGIN PGP SIGNATURE-----\n", fp);
    else if (mutt_strcmp("-----END PGP MESSAGE-----\n", buffer) == 0)
      fputs ("-----END PGP SIGNATURE-----\n", fp);
    else
      fputs (buffer, fp);
    empty = 0; /* got some output, so we're ok */
  }

  /* check for errors from PGP */
  err = 0;
  while (fgets (buffer, sizeof (buffer) - 1, pgperr) != NULL)
  {
    err = 1;
    fputs (buffer, stdout);
  }

  mutt_wait_filter (thepid);
  fclose (pgperr);
  fclose (pgpout);
  unlink (signedfile);
  
  if (fclose (fp) != 0)
  {
    mutt_perror ("fclose");
    unlink (sigfile);
    return (NULL);
  }

  if (err)
    mutt_any_key_to_continue (NULL);
  if (empty)
  {
    unlink (sigfile);
    return (NULL); /* fatal error while signing */
  }

  t = mutt_new_body ();
  t->type = TYPEMULTIPART;
  t->subtype = safe_strdup ("signed");
  t->use_disp = 0;
  t->encoding = ENC7BIT;

  mutt_set_parameter ("protocol", "application/pgp-signature", &t->parameter);
  mutt_set_parameter ("micalg", PgpSignMicalg, &t->parameter);
  mutt_generate_boundary (&t->parameter);

  t->parts = a;
  a = t;

  t->parts->next = mutt_new_body ();
  t = t->parts->next;
  t->type = TYPEAPPLICATION;
  t->subtype = safe_strdup ("pgp-signature");
  t->filename = safe_strdup (sigfile);
  t->use_disp = 0;
  t->encoding = ENC7BIT;
  t->unlink = 1; /* ok to remove this file after sending. */

  return (a);
}

/* This routine attempts to find the keyids of the recipients of a message.
 * It returns NULL if any of the keys can not be found.
 */
char *pgp_findKeys (ADDRESS *to, ADDRESS *cc, ADDRESS *bcc)
{
  char *keyID, *keylist = NULL;
  size_t keylist_size = 0;
  size_t keylist_used = 0;
  ADDRESS *tmp = NULL;
  ADDRESS **last = &tmp;
  ADDRESS *p;
  int i;
  pgp_key_t *k_info, *key;
  struct pgp_vinfo *pgp = pgp_get_vinfo (PGP_ENCRYPT);
  
  if (!pgp)
    return NULL;
  
  for (i = 0; i < 3; i++) 
  {
    switch (i)
    {
      case 0: p = to; break;
      case 1: p = cc; break;
      case 2: p = bcc; break;
      default: abort ();
    }
    
    *last = rfc822_cpy_adr (p);
    while (*last)
      last = &((*last)->next);
  }

  tmp = mutt_remove_duplicates (tmp);

  for (p = tmp; p ; p = p->next)
  {
    char buf[LONG_STRING];

    k_info = NULL;
    if ((keyID = mutt_pgp_hook (p)) != NULL)
    {
      snprintf (buf, sizeof (buf), _("Use keyID = \"%s\" for %s?"), keyID, p->mailbox);
      if (mutt_yesorno (buf, M_YES) == M_YES)
	k_info = pgp_getkeybystr (pgp, keyID, KEYFLAG_CANENCRYPT, PGP_PUBRING);
    }

    if (k_info == NULL && (k_info = pgp_getkeybyaddr (pgp, p, KEYFLAG_CANENCRYPT, PGP_PUBRING)) == NULL)
    {
      snprintf (buf, sizeof (buf), _("Enter keyID for %s: "), p->mailbox);
      
      if ((key = pgp_ask_for_key (pgp, buf, p->mailbox,
				  KEYFLAG_CANENCRYPT, PGP_PUBRING)) == NULL)
      {
	safe_free ((void **)&keylist);
	rfc822_free_address (&tmp);
	return NULL;
      }
    }
    else
      key = k_info;

    keyID = pgp_keyid (key);
    pgp_free_key (&key);
    
    keylist_size += mutt_strlen (keyID) + 4;
    safe_realloc ((void **)&keylist, keylist_size);
    sprintf (keylist + keylist_used, "%s0x%s", keylist_used ? " " : "",
	     keyID);
    keylist_used = mutt_strlen (keylist);
  }
  rfc822_free_address (&tmp);
  return (keylist);
}

/* Warning: "a" is no longer free()d in this routine, you need
 * to free() it later.  This is necessary for $fcc_attach. */

static BODY *pgp_encrypt_message (BODY *a, char *keylist, int sign)
{
  char buf[LONG_STRING];
  char tempfile[_POSIX_PATH_MAX], pgperrfile[_POSIX_PATH_MAX];
  char pgpinfile[_POSIX_PATH_MAX];
  FILE *pgpin, *pgperr, *fpout, *fptmp;
  BODY *t;
  int err = 0;
  int empty;
  pid_t thepid;
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_ENCRYPT);
  
  if(!pgp)
    return NULL;
  
  mutt_mktemp (tempfile);
  if ((fpout = safe_fopen (tempfile, "w+")) == NULL)
  {
    mutt_perror (tempfile);
    return (NULL);
  }

  mutt_mktemp (pgperrfile);
  if ((pgperr = safe_fopen (pgperrfile, "w+")) == NULL)
  {
    mutt_perror (pgperrfile);
    unlink(tempfile);
    fclose(fpout);
    return NULL;
  }
  unlink (pgperrfile);

  mutt_mktemp(pgpinfile);
  if((fptmp = safe_fopen(pgpinfile, "w")) == NULL)
  {
    mutt_perror(pgpinfile);
    unlink(tempfile);
    fclose(fpout);
    fclose(pgperr);
    return NULL;
  }
  
  if (sign)
    convert_to_7bit (a);
  
  mutt_write_mime_header (a, fptmp);
  fputc ('\n', fptmp);
  mutt_write_mime_body (a, fptmp);
  fclose(fptmp);
  
  if ((thepid = pgp->invoke_encrypt (pgp, &pgpin, NULL, NULL, -1, 
				     fileno (fpout), fileno (pgperr),
				     pgpinfile, keylist, sign)) == -1)
  {
    fclose (pgperr);
    unlink(pgpinfile);
    return (NULL);
  }

  if (sign)
  {
    fputs (PgpPass, pgpin);
    fputc ('\n', pgpin);
  }
  fclose(pgpin);
  
  mutt_wait_filter (thepid);
  unlink(pgpinfile);
  
  fflush (fpout);
  rewind (fpout);
  empty = (fgetc (fpout) == EOF);
  fclose (fpout);

  fflush (pgperr);
  rewind (pgperr);
  while (fgets (buf, sizeof (buf) - 1, pgperr) != NULL)
  {
    err = 1;
    fputs (buf, stdout);
  }
  fclose (pgperr);

  /* pause if there is any error output from PGP */
  if (err)
    mutt_any_key_to_continue (NULL);

  if (empty)
  {
    /* fatal error while trying to encrypt message */
    unlink (tempfile);
    return (NULL);
  }

  t = mutt_new_body ();
  t->type = TYPEMULTIPART;
  t->subtype = safe_strdup ("encrypted");
  t->encoding = ENC7BIT;
  t->use_disp = 0;

  mutt_set_parameter("protocol", "application/pgp-encrypted", &t->parameter);
  mutt_generate_boundary(&t->parameter);
  
  t->parts = mutt_new_body ();
  t->parts->type = TYPEAPPLICATION;
  t->parts->subtype = safe_strdup ("pgp-encrypted");
  t->parts->encoding = ENC7BIT;
  t->parts->use_disp = 0;

  t->parts->next = mutt_new_body ();
  t->parts->next->type = TYPEAPPLICATION;
  t->parts->next->subtype = safe_strdup ("octet-stream");
  t->parts->next->encoding = ENC7BIT;
  t->parts->next->filename = safe_strdup (tempfile);
  t->parts->next->use_disp = 0;
  t->parts->next->unlink = 1; /* delete after sending the message */

  return (t);
}

int pgp_get_keys (HEADER *msg, char **pgpkeylist)
{
  /* Do a quick check to make sure that we can find all of the encryption
   * keys if the user has requested this service.
   */

  set_option (OPTPGPCHECKTRUST);

  *pgpkeylist = NULL;
  if (msg->pgp & PGPENCRYPT)
  {
    if ((*pgpkeylist = pgp_findKeys (msg->env->to, msg->env->cc,
				      msg->env->bcc)) == NULL)
      return (-1);
  }

  return (0);
}

int pgp_protect (HEADER *msg, char *pgpkeylist)
{
  BODY *pbody = NULL;

  if ((msg->pgp & PGPSIGN) && !pgp_valid_passphrase ())
    return (-1);

  if (!isendwin ())
    endwin ();
  if (msg->pgp & PGPENCRYPT)
    pbody = pgp_encrypt_message (msg->content, pgpkeylist, msg->pgp & PGPSIGN);
  else if (msg->pgp & PGPSIGN)
    pbody = pgp_sign_message (msg->content);

  if (!pbody)
    return (-1);
  msg->content = pbody;
  return (0);
}

#endif /* _PGPPATH */
