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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
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
#include "copy.h"

#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#ifdef HAVE_PGP


char PgpPass[STRING];
static time_t PgpExptime = 0; /* when does the cached passphrase expire? */

void pgp_void_passphrase (void)
{
  memset (PgpPass, 0, sizeof (PgpPass));
  PgpExptime = 0;
}

# if defined(HAVE_SETRLIMIT) && (!defined(DEBUG))

static void disable_coredumps (void)
{
  struct rlimit rl = {0, 0};
  static short done = 0;

  if (!done)
  {
    setrlimit (RLIMIT_CORE, &rl);
    done = 1;
  }
}

# endif /* HAVE_SETRLIMIT */

int pgp_valid_passphrase (void)
{
  time_t now = time (NULL);

# if defined(HAVE_SETRLIMIT) && (!defined(DEBUG))
  disable_coredumps ();
# endif

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

  t = time (NULL);
  setlocale (LC_TIME, "");
  strftime (p, sizeof (p),
	    _("[-- PGP output follows (current time: %c) --]\n"),
	    localtime (&t));
  setlocale (LC_TIME, "C");
  state_puts (p, s);
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
	
	if ((thepid = pgp_invoke_decode (&pgpin, NULL,
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
    if (mutt_is_multipart_signed(m))
      t |= PGPSIGN;
    else if (mutt_is_multipart_encrypted(m))
      t |= PGPENCRYPT;

    if ((mutt_is_multipart_signed (m) || mutt_is_multipart_encrypted (m)) 
	&& m->goodsig)
      t |= PGPGOODSIGN;
  }

  if (m->type == TYPEMULTIPART || m->type == TYPEMESSAGE)
  {
    BODY *p;
 
    for (p = m->parts; p; p = p->next)
      t |= pgp_query(p) & ~PGPGOODSIGN;
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

static int pgp_verify_one (BODY *sigbdy, STATE *s, const char *tempfile)
{
  char sigfile[_POSIX_PATH_MAX], pgperrfile[_POSIX_PATH_MAX];
  FILE *fp, *pgpout, *pgperr;
  pid_t thepid;
  int badsig = -1;

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
  
  if((thepid = pgp_invoke_verify (NULL, &pgpout, NULL, 
				   -1, -1, fileno(pgperr),
				   tempfile, sigfile)) != -1)
  {
    if (PgpGoodSign.pattern)
    {
      char *line = NULL;
      int lineno = 0;
      size_t linelen;

      while ((line = mutt_read_line (line, &linelen, pgpout, &lineno)) != NULL)
      {
	if (regexec (PgpGoodSign.rx, line, 0, NULL, 0) == 0)
	  badsig = 0;

	fputs (line, s->fpout);
	fputc ('\n', s->fpout);
      }
      safe_free ((void **) &line);
    }
    else
    {
      mutt_copy_stream(pgpout, s->fpout);
      badsig = 0;
    }

    fclose (pgpout);
    fflush(pgperr);
    rewind(pgperr);
    mutt_copy_stream(pgperr, s->fpout);
    fclose(pgperr);
    
    if (mutt_wait_filter (thepid))
      badsig = -1;
  }
  
  state_puts (_("[-- End of PGP output --]\n\n"), s);
  
  mutt_unlink (sigfile);
  mutt_unlink (pgperrfile);

  return badsig;
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
  
  BODY *b = a;
  BODY **signatures = NULL;
  int sigcnt = 0;
  int i;
  short goodsig = 1;

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
    state_puts(_("[-- Error: Inconsistent multipart/signed structure! --]\n\n"), s);
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
	  {
	    if (pgp_verify_one (signatures[i], s, tempfile) != 0)
	      goodsig = 0;
	  }
	  else
	    state_printf (s, _("[-- Warning: We can't verify %s/%s signatures. --]\n\n"),
			  TYPE(signatures[i]), signatures[i]->subtype);
	}
      }
      
      mutt_unlink (tempfile);

      b->goodsig = goodsig;
      
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
  char tempfname[_POSIX_PATH_MAX];
  FILE *fpout;

  if (h)
  {
    mutt_parse_mime_message (Context, h);
    if(h->pgp & PGPENCRYPT && !pgp_valid_passphrase ())
      return;
  }

  mutt_mktemp (tempfname);
  if (!(fpout = safe_fopen (tempfname, "w")))
  {
    mutt_perror (tempfname);
    return;
  }

  set_option (OPTDONTHANDLEPGPKEYS);
  
  if (!h)
  {
    for (i = 0; i < Context->vcount; i++)
    {
      if (Context->hdrs[Context->v2r[i]]->tagged)
      {
	mutt_parse_mime_message (Context, Context->hdrs[Context->v2r[i]]);
	if (Context->hdrs[Context->v2r[i]]->pgp & PGPENCRYPT
	   && !pgp_valid_passphrase())
	{
	  fclose (fpout);
	  goto bailout;
	}
	mutt_copy_message (fpout, Context, Context->hdrs[Context->v2r[i]], 
			   M_CM_DECODE|M_CM_CHARCONV, 0);
      }
    }
  } 
  else
  {
    mutt_parse_mime_message (Context, h);
    if (h->pgp & PGPENCRYPT && !pgp_valid_passphrase())
    {
      fclose (fpout);
      goto bailout;
    }
    mutt_copy_message (fpout, Context, h, M_CM_DECODE|M_CM_CHARCONV, 0);
  }
      
  fclose (fpout);
  endwin ();
  pgp_invoke_import (tempfname);
  mutt_any_key_to_continue (NULL);

  bailout:
  
  mutt_unlink (tempfname);
  unset_option (OPTDONTHANDLEPGPKEYS);
  
}

static void pgp_extract_keys_from_attachment (FILE *fp, BODY *top)
{
  STATE s;
  FILE *tempfp;
  char tempfname[_POSIX_PATH_MAX];

  mutt_mktemp (tempfname);
  if (!(tempfp = safe_fopen (tempfname, "w")))
  {
    mutt_perror (tempfname);
    return;
  }

  memset (&s, 0, sizeof (STATE));
  
  s.fpin = fp;
  s.fpout = tempfp;
  
  mutt_body_handler (top, &s);

  fclose (tempfp);

  pgp_invoke_import (tempfname);
  mutt_any_key_to_continue (NULL);

  mutt_unlink (tempfname);
}

void pgp_extract_keys_from_attachment_list (FILE *fp, int tag, BODY *top)
{
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
      pgp_extract_keys_from_attachment (fp, top);
    
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

  if ((thepid = pgp_invoke_decrypt (&pgpin, &pgpout, NULL, -1, -1,
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
      strcpy (buf + len - 2, "\n");	/* __STRCPY_CHECKED__ */
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

    /* 
     * if a multipart/signed is the _only_ sub-part of a
     * multipart/encrypted, cache signature verification
     * status.
     *
     */
    
    if (mutt_is_multipart_signed (tattach) && !tattach->next)
      a->goodsig = tattach->goodsig;
    
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
    else if (a->content && a->encoding != ENCBASE64 &&
	     (a->content->from || (a->content->space && 
				   option (OPTPGPSTRICTENC))))
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
  
  if ((thepid = pgp_invoke_sign (&pgpin, &pgpout, &pgperr,
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
  t->encoding = ENC7BIT;
  t->use_disp = 0;
  t->disposition = DISPINLINE;

  mutt_generate_boundary (&t->parameter);
  mutt_set_parameter ("protocol", "application/pgp-signature", &t->parameter);
  mutt_set_parameter ("micalg", PgpSignMicalg, &t->parameter);

  t->parts = a;
  a = t;

  t->parts->next = mutt_new_body ();
  t = t->parts->next;
  t->type = TYPEAPPLICATION;
  t->subtype = safe_strdup ("pgp-signature");
  t->filename = safe_strdup (sigfile);
  t->use_disp = 0;
  t->disposition = DISPINLINE;
  t->encoding = ENC7BIT;
  t->unlink = 1; /* ok to remove this file after sending. */

  return (a);
}

/* This routine attempts to find the keyids of the recipients of a message.
 * It returns NULL if any of the keys can not be found.
 */
char *pgp_findKeys (ADDRESS *to, ADDRESS *cc, ADDRESS *bcc)
{
  char *keyID, *keylist = NULL, *t;
  size_t keylist_size = 0;
  size_t keylist_used = 0;
  ADDRESS *tmp = NULL, *addr = NULL;
  ADDRESS **last = &tmp;
  ADDRESS *p, *q;
  int i;
  pgp_key_t *k_info, *key;

  const char *fqdn = mutt_fqdn (1);
  
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

  if (fqdn)
    rfc822_qualify (tmp, fqdn);

  tmp = mutt_remove_duplicates (tmp);
  
  for (p = tmp; p ; p = p->next)
  {
    char buf[LONG_STRING];

    q = p;
    k_info = NULL;

    if ((keyID = mutt_pgp_hook (p)) != NULL)
    {
      int r;
      snprintf (buf, sizeof (buf), _("Use keyID = \"%s\" for %s?"), keyID, p->mailbox);
      if ((r = mutt_yesorno (buf, M_YES)) == M_YES)
      {
	/* check for e-mail address */
	if ((t = strchr (keyID, '@')) && 
	    (addr = rfc822_parse_adrlist (NULL, keyID)))
	{
	  if (fqdn) rfc822_qualify (addr, fqdn);
	  q = addr;
	}
	else
	  k_info = pgp_getkeybystr (keyID, KEYFLAG_CANENCRYPT, PGP_PUBRING);
      }
      else if (r == -1)
      {
	safe_free ((void **) &keylist);
	rfc822_free_address (&tmp);
	rfc822_free_address (&addr);
	return NULL;
      }
    }

    if (k_info == NULL)
      pgp_invoke_getkeys (q);

    if (k_info == NULL && (k_info = pgp_getkeybyaddr (q, KEYFLAG_CANENCRYPT, PGP_PUBRING)) == NULL)
    {
      snprintf (buf, sizeof (buf), _("Enter keyID for %s: "), q->mailbox);

      if ((key = pgp_ask_for_key (buf, q->mailbox,
				  KEYFLAG_CANENCRYPT, PGP_PUBRING)) == NULL)
      {
	safe_free ((void **)&keylist);
	rfc822_free_address (&tmp);
	rfc822_free_address (&addr);
	return NULL;
      }
    }
    else
      key = k_info;

    keyID = pgp_keyid (key);
    
    keylist_size += mutt_strlen (keyID) + 4;
    safe_realloc ((void **)&keylist, keylist_size);
    sprintf (keylist + keylist_used, "%s0x%s", keylist_used ? " " : "",	/* __SPRINTF_CHECKED__ */
	     keyID);
    keylist_used = mutt_strlen (keylist);

    pgp_free_key (&key);
    rfc822_free_address (&addr);

  }
  rfc822_free_address (&tmp);
  return (keylist);
}

/* Warning: "a" is no longer freed in this routine, you need
 * to free it later.  This is necessary for $fcc_attach. */

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
  
  if ((thepid = pgp_invoke_encrypt (&pgpin, NULL, NULL, -1, 
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
  t->disposition = DISPINLINE;

  mutt_generate_boundary(&t->parameter);
  mutt_set_parameter("protocol", "application/pgp-encrypted", &t->parameter);
  
  t->parts = mutt_new_body ();
  t->parts->type = TYPEAPPLICATION;
  t->parts->subtype = safe_strdup ("pgp-encrypted");
  t->parts->encoding = ENC7BIT;
  t->parts->use_disp = 1;
  t->parts->disposition = DISPINLINE;
  t->parts->d_filename = safe_strdup ("msg.asc"); /* non pgp/mime can save */

  t->parts->next = mutt_new_body ();
  t->parts->next->type = TYPEAPPLICATION;
  t->parts->next->subtype = safe_strdup ("octet-stream");
  t->parts->next->encoding = ENC7BIT;
  t->parts->next->filename = safe_strdup (tempfile);
  t->parts->next->use_disp = 0;
  t->parts->next->disposition = DISPINLINE;
  t->parts->next->unlink = 1; /* delete after sending the message */

  return (t);
}

static BODY *pgp_traditional_encryptsign (BODY *a, int flags, char *keylist)
{
  BODY *b;

  char pgpoutfile[_POSIX_PATH_MAX];
  char pgperrfile[_POSIX_PATH_MAX];
  char pgpinfile[_POSIX_PATH_MAX];
  
  FILE *pgpout = NULL, *pgperr = NULL, *pgpin = NULL;
  FILE *fp;

  int empty;
  int err;

  char buff[STRING];

  pid_t thepid;
  
  if ((fp = fopen (a->filename, "r")) == NULL)
  {
    mutt_perror (a->filename);
    return NULL;
  }
  
  mutt_mktemp (pgpinfile);
  if ((pgpin = safe_fopen (pgpinfile, "w")) == NULL)
  {
    mutt_perror (pgpinfile);
    fclose (fp);
    return NULL;
  }

  mutt_copy_stream (fp, pgpin);
  fclose (fp);
  fclose (pgpin);

  mutt_mktemp (pgpoutfile);
  mutt_mktemp (pgperrfile);
  if ((pgpout = safe_fopen (pgpoutfile, "w+")) == NULL ||
      (pgperr = safe_fopen (pgperrfile, "w+")) == NULL)
  {
    mutt_perror (pgpout ? pgperrfile : pgpoutfile);
    unlink (pgpinfile);
    if (pgpout) 
    {
      fclose (pgpout);
      unlink (pgpoutfile);
    }
    return NULL;
  }
  
  unlink (pgperrfile);
  
  if ((thepid = pgp_invoke_traditional (&pgpin, NULL, NULL, 
					-1, fileno (pgpout), fileno (pgperr),
					pgpinfile, keylist, flags)) == -1)
  {
    mutt_perror _("Can't invoke PGP");
    fclose (pgpout);
    fclose (pgperr);
    mutt_unlink (pgpinfile);
    unlink (pgpoutfile);
    return NULL;
  }

  if (flags & PGPSIGN)
    fprintf (pgpin, "%s\n", PgpPass);
  fclose (pgpin);

  mutt_wait_filter (thepid);

  mutt_unlink (pgpinfile);

  fflush (pgpout);
  fflush (pgperr);

  rewind (pgpout);
  rewind (pgperr);
  
  empty = (fgetc (pgpout) == EOF);
  fclose (pgpout);
  
  err = 0;
  
  while (fgets (buff, sizeof (buff), pgperr))
  {
    err = 1;
    fputs (buff, stdout);
  }
  
  fclose (pgperr);
  
  if (err)
    mutt_any_key_to_continue (NULL);
  
  if (empty)
  {
    unlink (pgpoutfile);
    return NULL;
  }
    
  b = mutt_new_body ();
  
  b->encoding = ENC7BIT;

  b->type = TYPEAPPLICATION;
  b->subtype = safe_strdup ("pgp");

  mutt_set_parameter ("format", "text", &b->parameter);
  mutt_set_parameter ("x-action", flags & PGPENCRYPT ? "encrypt" : "sign",
		      &b->parameter);

  b->filename = safe_strdup (pgpoutfile);
  
  /* The following is intended to give a clue to some completely brain-dead 
   * "mail environments" which are typically used by large corporations.
   */

  b->d_filename = safe_strdup ("msg.pgp");
  b->disposition = DISPINLINE;
  b->unlink   = 1;
  b->use_disp = 1;

  return b;
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
  int flags = msg->pgp;
  int traditional = 0;
  int i;

  if ((msg->pgp & PGPSIGN) && !pgp_valid_passphrase ())
    return (-1);

  if ((msg->content->type == TYPETEXT) &&
      !mutt_strcasecmp (msg->content->subtype, "plain") &&
      ((flags & PGPENCRYPT) || (msg->content->content && msg->content->content->hibin == 0)))
  {
    if ((i = query_quadoption (OPT_PGPTRADITIONAL, _("Create an application/pgp message?"))) == -1)
      return -1;
    else if (i == M_YES)
      traditional = 1;
  }

  mutt_message _("Invoking PGP...");

  if (!isendwin ())
    endwin ();

  if (traditional)
  {
    if (!(pbody = pgp_traditional_encryptsign (msg->content, flags, pgpkeylist)))
      return -1;
    
    msg->content = pbody;
    return 0;
  }

  if ((flags & PGPSIGN) && (!(flags & PGPENCRYPT) || option (OPTPGPRETAINABLESIG)))
  {
    if (!(pbody = pgp_sign_message (msg->content)))
      return -1;

    msg->content = pbody;
    flags &= ~PGPSIGN;
  }

  if (flags & PGPENCRYPT)
  {
    if (!(pbody = pgp_encrypt_message (msg->content, pgpkeylist, flags & PGPSIGN)))
    {

      /* did we perform a retainable signature? */
      if (flags != msg->pgp)
      {
	/* remove the outer multipart layer */
	msg->content = mutt_remove_multipart (msg->content);
	/* get rid of the signature */
	mutt_free_body (&msg->content->next);
      }

      return (-1);
    }

    /* destroy temporary signature envelope when doing retainable 
     * signatures.
     */
    if (flags != msg->pgp)
    {
      mutt_remove_multipart (msg->content);
      mutt_free_body (&msg->content->next);
    }
    
    msg->content = pbody;
  }

  return (0);
}

#endif /* HAVE_PGP */
