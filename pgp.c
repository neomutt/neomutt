/*
 * Copyright (C) 1996,1997 Michael R. Elkins <me@mutt.org>
 * Copyright (c) 1998,1999 Thomas Roessler <roessler@does-not-exist.org>
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

#include "crypt.h"



char PgpPass[STRING];
time_t PgpExptime = 0; /* when does the cached passphrase expire? */

void pgp_void_passphrase (void)
{
  memset (PgpPass, 0, sizeof (PgpPass));
  PgpExptime = 0;
}


void pgp_forget_passphrase (void)
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



/* Copy PGP output messages and look for signs of a good signature */

static int pgp_copy_checksig (FILE *fpin, FILE *fpout)
{
  int rv = -1;

  if (PgpGoodSign.pattern)
  {
    char *line = NULL;
    int lineno = 0;
    size_t linelen;
    
    while ((line = mutt_read_line (line, &linelen, fpin, &lineno)) != NULL)
    {
      if (regexec (PgpGoodSign.rx, line, 0, NULL, 0) == 0)
      {
	dprint (2, (debugfile, "pgp_copy_checksig: \"%s\" matches regexp.\n",
		    line));
	rv = 0;
      }
      else
	dprint (2, (debugfile, "pgp_copy_checksig: \"%s\" doesn't match regexp.\n",
		    line));
      
      fputs (line, fpout);
      fputc ('\n', fpout);
    }
    FREE (&line);
  }
  else
  {
    dprint (2, (debugfile, "pgp_copy_checksig: No pattern.\n"));
    mutt_copy_stream (fpin, fpout);
    rv = 1;
  }

  return rv;
}

/* 
 * Copy a clearsigned message, and strip the signature and PGP's
 * dash-escaping.
 * 
 * XXX - charset handling: We assume that it is safe to do
 * character set decoding first, dash decoding second here, while
 * we do it the other way around in the main handler.
 * 
 * (Note that we aren't worse than Outlook &c in this, and also
 * note that we can successfully handle anything produced by any
 * existing versions of mutt.) 
 */

static void pgp_copy_clearsigned (FILE *fpin, STATE *s, char *charset)
{
  char buf[HUGE_STRING];
  short complete, armor_header;
  
  FGETCONV *fc;
  
  rewind (fpin);
  
  fc = fgetconv_open (fpin, charset, Charset, M_ICONV_HOOK_FROM);
  
  for (complete = 1, armor_header = 1;
       fgetconvs (buf, sizeof (buf), fc) != NULL;
       complete = strchr (buf, '\n') != NULL)
  {
    if (!complete)
    {
      if (!armor_header)
	state_puts (buf, s);
      continue;
    }

    if (mutt_strcmp (buf, "-----BEGIN PGP SIGNATURE-----\n") == 0)
      break;
    
    if (armor_header)
    {
      if (buf[0] == '\n') 
	armor_header = 0;
      continue;
    }
    
    if (s->prefix) 
      state_puts (s->prefix, s);
    
    if (buf[0] == '-' && buf[1] == ' ')
      state_puts (buf + 2, s);
    else
      state_puts (buf, s);
  }
  
  fgetconv_close (&fc);
}


/* Support for the Application/PGP Content Type. */

void pgp_application_pgp_handler (BODY *m, STATE *s)
{
  int needpass = -1, pgp_keyblock = 0;
  int clearsign = 0, rv, rc;
  long start_pos = 0;
  long bytes, last_pos, offset;
  char buf[HUGE_STRING];
  char outfile[_POSIX_PATH_MAX];
  char tmpfname[_POSIX_PATH_MAX];
  FILE *pgpout = NULL, *pgpin = NULL, *pgperr = NULL;
  FILE *tmpfp;
  pid_t thepid;

  short maybe_goodsig = 1;
  short have_any_sigs = 0;

  char body_charset[STRING];
  mutt_get_body_charset (body_charset, sizeof (body_charset), m);
  
  fseek (s->fpin, m->offset, 0);
  last_pos = m->offset;
  
  for (bytes = m->length; bytes > 0;)
  {
    if (fgets (buf, sizeof (buf), s->fpin) == NULL)
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
      else if (!option (OPTDONTHANDLEPGPKEYS) &&
	       mutt_strcmp ("PUBLIC KEY BLOCK-----\n", buf + 15) == 0)
      {
        needpass = 0;
        pgp_keyblock =1;
      } 
      else
      {
	/* XXX - we may wish to recode here */
	if (s->prefix)
	  state_puts (s->prefix, s);
	state_puts (buf, s);
	continue;
      }

      have_any_sigs = have_any_sigs || (clearsign && (s->flags & M_VERIFY));

      /* Copy PGP material to temporary file */
      mutt_mktemp (tmpfname);
      if ((tmpfp = safe_fopen (tmpfname, "w+")) == NULL)
      {
	mutt_perror (tmpfname);
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

      /* leave tmpfp open in case we still need it - but flush it! */
      fflush (tmpfp);
      
      
      /* Invoke PGP if needed */
      if (!clearsign || (s->flags & M_VERIFY))
      {
	mutt_mktemp (outfile);
	if ((pgpout = safe_fopen (outfile, "w+")) == NULL)
	{
	  mutt_perror (tmpfname);
	  return;
	}
	
	if ((thepid = pgp_invoke_decode (&pgpin, NULL, &pgperr, -1,
					 fileno (pgpout), -1, tmpfname,
					 needpass)) == -1)
	{
	  safe_fclose (&pgpout);
	  maybe_goodsig = 0;
	  pgpin = NULL;
	  pgperr = NULL;
	  state_attach_puts (_("[-- Error: unable to create PGP subprocess! --]\n"), s);
	}
	else /* PGP started successfully */
	{
	  if (needpass)
	  {
	    if (!pgp_valid_passphrase ()) pgp_void_passphrase();
	    fprintf (pgpin, "%s\n", PgpPass);
	  }
	  
	  safe_fclose (&pgpin);

	  rv = mutt_wait_filter (thepid);

	  if (s->flags & M_DISPLAY)
	  {
	    crypt_current_time (s, "PGP");
	    rc = pgp_copy_checksig (pgperr, s->fpout);
	    if (rc == 0) have_any_sigs = 1;
	    if (rc || rv) maybe_goodsig = 0;
	  }
	  
	  safe_fclose (&pgperr);
	  
	  if (s->flags & M_DISPLAY)
	  {
	    state_putc ('\n', s);
	    state_attach_puts (_("[-- End of PGP output --]\n\n"), s);
	  }
	}
      }
      

      /*
       * Now, copy cleartext to the screen.  NOTE - we expect that PGP
       * outputs utf-8 cleartext.  This may not always be true, but it 
       * seems to be a reasonable guess.
       */

      if(s->flags & M_DISPLAY)
      {
	if (needpass)
	  state_attach_puts (_("[-- BEGIN PGP MESSAGE --]\n\n"), s);
	else if (pgp_keyblock)
	  state_attach_puts (_("[-- BEGIN PGP PUBLIC KEY BLOCK --]\n"), s);
	else
	  state_attach_puts (_("[-- BEGIN PGP SIGNED MESSAGE --]\n\n"), s);
      }

      if (clearsign)
      {
	rewind (tmpfp);
	if (tmpfp) 
	  pgp_copy_clearsigned (tmpfp, s, body_charset);
      }
      else if (pgpout)
      {
	FGETCONV *fc;
	int c;
	rewind (pgpout);
	fc = fgetconv_open (pgpout, "utf-8", Charset, 0);
	while ((c = fgetconv (fc)) != EOF)
	{
	  state_putc (c, s);
	  if (c == '\n' && s->prefix)
	    state_puts (s->prefix, s);
	}
	fgetconv_close (&fc);
      }

      if (s->flags & M_DISPLAY)
      {
	state_putc ('\n', s);
	if (needpass)
	  state_attach_puts (_("[-- END PGP MESSAGE --]\n"), s);
	else if (pgp_keyblock)
	  state_attach_puts (_("[-- END PGP PUBLIC KEY BLOCK --]\n"), s);
	else
	  state_attach_puts (_("[-- END PGP SIGNED MESSAGE --]\n"), s);
      }

      if (tmpfp)
      {
	safe_fclose (&tmpfp);
	mutt_unlink (tmpfname);
      }
      if (pgpout)
      {
	safe_fclose (&pgpout);
	mutt_unlink (outfile);
      }
    }
    else
    {
      /* XXX - we may wish to recode here */
      if (s->prefix)
	state_puts (s->prefix, s);
      state_puts (buf, s);
    }
  }

  m->goodsig = (maybe_goodsig && have_any_sigs);

  if (needpass == -1)
  {
    state_attach_puts (_("[-- Error: could not find beginning of PGP message! --]\n\n"), s);
    return;
  }
}

static int pgp_check_traditional_one_body (FILE *fp, BODY *b, int tagged_only)
{
  char tempfile[_POSIX_PATH_MAX];
  char buf[HUGE_STRING];
  FILE *tfp;
  
  short sgn = 0;
  short enc = 0;
  
  if (b->type != TYPETEXT)
    return 0;

  if (tagged_only && !b->tagged)
    return 0;

  mutt_mktemp (tempfile);
  if (mutt_decode_save_attachment (fp, b, tempfile, 0, 0) != 0)
  {
    unlink (tempfile);
    return 0;
  }
  
  if ((tfp = fopen (tempfile, "r")) == NULL)
  {
    unlink (tempfile);
    return 0;
  }
  
  while (fgets (buf, sizeof (buf), tfp))
  {
    if (mutt_strncmp ("-----BEGIN PGP ", buf, 15) == 0)
    {
      if (mutt_strcmp ("MESSAGE-----\n", buf + 15) == 0)
	enc = 1;
      else if (mutt_strcmp ("SIGNED MESSAGE-----\n", buf + 15) == 0)
	sgn = 1;
    }
  }
  safe_fclose (&tfp);
  unlink (tempfile);

  if (!enc && !sgn)
    return 0;

  /* fix the content type */
  
  mutt_set_parameter ("format", "fixed", &b->parameter);
  mutt_set_parameter ("x-action", enc ? "pgp-encrypted" : "pgp-signed", &b->parameter);
  
  return 1;
}

int pgp_check_traditional (FILE *fp, BODY *b, int tagged_only)
{
  int rv = 0;
  int r;
  for (; b; b = b->next)
  {
    if (is_multipart (b))
      rv = pgp_check_traditional (fp, b->parts, tagged_only) || rv;
    else if (b->type == TYPETEXT)
    {
      if ((r = mutt_is_application_pgp (b)))
	rv = rv || r;
      else
	rv = pgp_check_traditional_one_body (fp, b, tagged_only) || rv;
    }
  }

  return rv;
}

     
int pgp_is_multipart_encrypted (BODY *b)
{
  char *p;
  
  if (!b || b->type != TYPEMULTIPART ||
      !b->subtype || ascii_strcasecmp (b->subtype, "encrypted") ||
      !(p = mutt_get_parameter ("protocol", b->parameter)) ||
      ascii_strcasecmp (p, "application/pgp-encrypted"))
    return 0;
  
  return PGPENCRYPT;
}

int mutt_is_application_pgp (BODY *m)
{
  int t = 0;
  char *p;
  
  if (m->type == TYPEAPPLICATION)
  {
    if (!ascii_strcasecmp (m->subtype, "pgp") || !ascii_strcasecmp (m->subtype, "x-pgp-message"))
    {
      if ((p = mutt_get_parameter ("x-action", m->parameter))
	  && (!ascii_strcasecmp (p, "sign") || !ascii_strcasecmp (p, "signclear")))
	t |= PGPSIGN;

      if ((p = mutt_get_parameter ("format", m->parameter)) && 
	  !ascii_strcasecmp (p, "keys-only"))
	t |= PGPKEY;

      if(!t) t |= PGPENCRYPT;  /* not necessarily correct, but... */
    }

    if (!ascii_strcasecmp (m->subtype, "pgp-signed"))
      t |= PGPSIGN;

    if (!ascii_strcasecmp (m->subtype, "pgp-keys"))
      t |= PGPKEY;
  }
  else if (m->type == TYPETEXT && ascii_strcasecmp ("plain", m->subtype) == 0)
  {
    if (((p = mutt_get_parameter ("x-mutt-action", m->parameter))
	 || (p = mutt_get_parameter ("x-action", m->parameter)) 
	 || (p = mutt_get_parameter ("action", m->parameter)))
	 && !ascii_strncasecmp ("pgp-sign", p, 8))
      t |= PGPSIGN;
    else if (p && !ascii_strncasecmp ("pgp-encrypt", p, 11))
      t |= PGPENCRYPT;
  }
  return t;
}




int pgp_verify_one (BODY *sigbdy, STATE *s, const char *tempfile)
{
  char sigfile[_POSIX_PATH_MAX], pgperrfile[_POSIX_PATH_MAX];
  FILE *fp, *pgpout, *pgperr;
  pid_t thepid;
  int badsig = -1;
  int rv;
  
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
  
  crypt_current_time (s, "PGP");
  
  if((thepid = pgp_invoke_verify (NULL, &pgpout, NULL, 
				   -1, -1, fileno(pgperr),
				   tempfile, sigfile)) != -1)
  {
    if (pgp_copy_checksig (pgpout, s->fpout) >= 0)
      badsig = 0;
    
    
    safe_fclose (&pgpout);
    fflush (pgperr);
    rewind (pgperr);
    
    if (pgp_copy_checksig  (pgperr, s->fpout) >= 0)
      badsig = 0;

    if ((rv = mutt_wait_filter (thepid)))
      badsig = -1;
    
     dprint (1, (debugfile, "pgp_verify_one: mutt_wait_filter returned %d.\n", rv));
  }

  safe_fclose (&pgperr);

  state_attach_puts (_("[-- End of PGP output --]\n\n"), s);

  mutt_unlink (sigfile);
  mutt_unlink (pgperrfile);

  dprint (1, (debugfile, "pgp_verify_one: returning %d.\n", badsig));
  
  return badsig;
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
    if(h->security & PGPENCRYPT && !pgp_valid_passphrase ())
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
	if (Context->hdrs[Context->v2r[i]]->security & PGPENCRYPT
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
    if (h->security & PGPENCRYPT && !pgp_valid_passphrase())
    {
      fclose (fpout);
      goto bailout;
    }
    mutt_copy_message (fpout, Context, h, M_CM_DECODE|M_CM_CHARCONV, 0);
  }
      
  fclose (fpout);
  mutt_endwin (NULL);
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
    mutt_error _("Internal error. Inform <roessler@does-not-exist.org>.");
    return;
  }

  mutt_endwin (NULL);
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

BODY *pgp_decrypt_part (BODY *a, STATE *s, FILE *fpout, BODY *p)
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
      state_attach_puts (_("[-- Error: could not create a PGP subprocess! --]\n\n"), s);
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
    if (pgp_copy_checksig (pgperr, s->fpout) == 0 && p)
      p->goodsig = 1;
    state_attach_puts (_("[-- End of PGP output --]\n\n"), s);
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
  BODY *p = b;
  
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

  *cur = pgp_decrypt_part (b, &s, *fpout, p);

  rewind (*fpout);
  return (0);
}

void pgp_encrypted_handler (BODY *a, STATE *s)
{
  char tempfile[_POSIX_PATH_MAX];
  FILE *fpout, *fpin;
  BODY *tattach;
  BODY *p = a;
  
  a = a->parts;
  if (!a || a->type != TYPEAPPLICATION || !a->subtype || 
      ascii_strcasecmp ("pgp-encrypted", a->subtype) != 0 ||
      !a->next || a->next->type != TYPEAPPLICATION || !a->next->subtype ||
      ascii_strcasecmp ("octet-stream", a->next->subtype) != 0)
  {
    if (s->flags & M_DISPLAY)
      state_attach_puts (_("[-- Error: malformed PGP/MIME message! --]\n\n"), s);
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
      state_attach_puts (_("[-- Error: could not create temporary file! --]\n"), s);
    return;
  }

  if (s->flags & M_DISPLAY) crypt_current_time (s, "PGP");

  if ((tattach = pgp_decrypt_part (a, s, fpout, p)) != NULL)
  {
    if (s->flags & M_DISPLAY)
      state_attach_puts (_("[-- The following data is PGP/MIME encrypted --]\n\n"), s);

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
      p->goodsig |= tattach->goodsig;
    
    if (s->flags & M_DISPLAY)
    {
      state_puts ("\n", s);
      state_attach_puts (_("[-- End of PGP/MIME encrypted data --]\n"), s);
    }

    mutt_free_body (&tattach);
  }

  fclose (fpout);
  mutt_unlink(tempfile);
}

/* ----------------------------------------------------------------------------
 * Routines for sending PGP/MIME messages.
 */


BODY *pgp_sign_message (BODY *a)
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
  mutt_set_parameter ("micalg", pgp_micalg (sigfile), &t->parameter);

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

static short is_numerical_keyid (const char *s)
{
  /* or should we require the "0x"? */
  if (strncmp (s, "0x", 2) == 0)
    s += 2;
  if (strlen (s) % 8)
    return 0;
  while (*s)
    if (strchr ("0123456789ABCDEFabcdef", *s++) == NULL)
      return 0;
  
  return 1;
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

    if ((keyID = mutt_crypt_hook (p)) != NULL)
    {
      int r;
      snprintf (buf, sizeof (buf), _("Use keyID = \"%s\" for %s?"), keyID, p->mailbox);
      if ((r = mutt_yesorno (buf, M_YES)) == M_YES)
      {
	if (is_numerical_keyid (keyID))
	{
	  if (strncmp (keyID, "0x", 2) == 0)
	    keyID += 2;
	  goto bypass_selection;		/* you don't see this. */
	}
	
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
	FREE (&keylist);
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
	FREE (&keylist);
	rfc822_free_address (&tmp);
	rfc822_free_address (&addr);
	return NULL;
      }
    }
    else
      key = k_info;

    keyID = pgp_keyid (key);
    
  bypass_selection:
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

BODY *pgp_encrypt_message (BODY *a, char *keylist, int sign)
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

  t->parts->next = mutt_new_body ();
  t->parts->next->type = TYPEAPPLICATION;
  t->parts->next->subtype = safe_strdup ("octet-stream");
  t->parts->next->encoding = ENC7BIT;
  t->parts->next->filename = safe_strdup (tempfile);
  t->parts->next->use_disp = 1;
  t->parts->next->disposition = DISPINLINE;
  t->parts->next->unlink = 1; /* delete after sending the message */
  t->parts->next->d_filename = safe_strdup ("msg.asc"); /* non pgp/mime can save */

  return (t);
}

BODY *pgp_traditional_encryptsign (BODY *a, int flags, char *keylist)
{
  BODY *b;

  char pgpoutfile[_POSIX_PATH_MAX];
  char pgperrfile[_POSIX_PATH_MAX];
  char pgpinfile[_POSIX_PATH_MAX];
  
  char body_charset[STRING];
  char *from_charset;
  const char *send_charset;
  
  FILE *pgpout = NULL, *pgperr = NULL, *pgpin = NULL;
  FILE *fp;

  int empty;
  int err;

  char buff[STRING];

  pid_t thepid;

  if (a->type != TYPETEXT)
    return NULL;
  if (ascii_strcasecmp (a->subtype, "plain"))
    return NULL;
  
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

  /* The following code is really correct:  If noconv is set,
   * a's charset parameter contains the on-disk character set, and
   * we have to convert from that to utf-8.  If noconv is not set,
   * we have to convert from $charset to utf-8.
   */
  
  mutt_get_body_charset (body_charset, sizeof (body_charset), a);
  if (a->noconv)
    from_charset = body_charset;
  else 
    from_charset = Charset;
    
  if (!mutt_is_us_ascii (body_charset))
  {
    int c;
    FGETCONV *fc;
    
    if (flags & ENCRYPT)
      send_charset = "us-ascii";
    else
      send_charset = "utf-8";
    
    fc = fgetconv_open (fp, from_charset, "utf-8", M_ICONV_HOOK_FROM);
    while ((c = fgetconv (fc)) != EOF)
      fputc (c, pgpin);
    
    fgetconv_close (&fc);
  }
  else
  {
    send_charset = "us-ascii";
    mutt_copy_stream (fp, pgpin);
  }
  safe_fclose (&fp);
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

  if (flags & SIGN)
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

  b->type = TYPETEXT;
  b->subtype = safe_strdup ("plain");
  
  mutt_set_parameter ("x-action", flags & ENCRYPT ? "pgp-encrypted" : "pgp-signed",
		      &b->parameter);
  mutt_set_parameter ("charset", send_charset, &b->parameter);
  
  b->filename = safe_strdup (pgpoutfile);
  
#if 0
  /* The following is intended to give a clue to some completely brain-dead 
   * "mail environments" which are typically used by large corporations.
   */

  b->d_filename = safe_strdup ("msg.pgp");
  b->use_disp = 1;

#endif

  b->disposition = DISPINLINE;
  b->unlink   = 1;

  b->noconv = 1;
  b->use_disp = 0;
  
  if (!(flags & ENCRYPT))
    b->encoding = a->encoding;
  
  return b;
}




#endif /* HAVE_PGP */
