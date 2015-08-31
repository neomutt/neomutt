/*
 * Copyright (C) 1996-7,2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1998-2005 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2004 g10 Code GmbH
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

/*
 * This file contains all of the PGP routines necessary to sign, encrypt,
 * verify and decrypt PGP messages in either the new PGP/MIME format, or
 * in the older Application/Pgp format.  It also contains some code to
 * cache the user's passphrase for repeat use when decrypting or signing
 * a message.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_crypt.h"
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

#ifdef CRYPT_BACKEND_CLASSIC_PGP

#include "mutt_crypt.h"
#include "mutt_menu.h"


char PgpPass[LONG_STRING];
time_t PgpExptime = 0; /* when does the cached passphrase expire? */

void pgp_void_passphrase (void)
{
  memset (PgpPass, 0, sizeof (PgpPass));
  PgpExptime = 0;
}

int pgp_valid_passphrase (void)
{
  time_t now = time (NULL);

  if (pgp_use_gpg_agent())
    {
      *PgpPass = 0;
      return 1; /* handled by gpg-agent */
    }

  if (now < PgpExptime)
    /* Use cached copy.  */
    return 1;
  
  pgp_void_passphrase ();

  if (mutt_get_password (_("Enter PGP passphrase:"), PgpPass, sizeof (PgpPass)) == 0)
    {
      PgpExptime = time (NULL) + PgpTimeout;
      return (1);
    }
  else
    PgpExptime = 0;

  return 0;
}

void pgp_forget_passphrase (void)
{
  pgp_void_passphrase ();
  mutt_message _("PGP passphrase forgotten.");
}

int pgp_use_gpg_agent (void)
{
  char *tty;

  /* GnuPG 2.1 no longer exports GPG_AGENT_INFO */
  if (!option (OPTUSEGPGAGENT))
    return 0;

  if ((tty = ttyname(0)))
    setenv("GPG_TTY", tty, 0);

  return 1;
}

static pgp_key_t _pgp_parent(pgp_key_t k)
{
  if((k->flags & KEYFLAG_SUBKEY) && k->parent && option(OPTPGPIGNORESUB))
    k = k->parent;

  return k;
}

char *pgp_long_keyid(pgp_key_t k)
{
  k = _pgp_parent(k);

  return k->keyid;
}

char *pgp_short_keyid(pgp_key_t k)
{
  k = _pgp_parent(k);

  return k->keyid + 8;
}

char *pgp_keyid(pgp_key_t k)
{
  k = _pgp_parent(k);

  return _pgp_keyid(k);
}

char *_pgp_keyid(pgp_key_t k)
{
  if(option(OPTPGPLONGIDS))
    return k->keyid;
  else
    return (k->keyid + 8);
}

char *pgp_fingerprint(pgp_key_t k)
{
  k = _pgp_parent(k);

  return k->fingerprint;
}

/* Grab the longest key identifier available: fingerprint or else
 * the long keyid.
 *
 * The longest available should be used for internally identifying
 * the key and for invoking pgp commands.
 */
char *pgp_fpr_or_lkeyid(pgp_key_t k)
{
  char *fingerprint;

  fingerprint = pgp_fingerprint (k);
  return fingerprint ? fingerprint : pgp_long_keyid (k);
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
    
    while ((line = mutt_read_line (line, &linelen, fpin, &lineno, 0)) != NULL)
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
      
      if (strncmp (line, "[GNUPG:] ", 9) == 0)
	continue;
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

  /* fromcode comes from the MIME Content-Type charset label. It might
   * be a wrong label, so we want the ability to do corrections via
   * charset-hooks. Therefore we set flags to M_ICONV_HOOK_FROM.
   */
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
      char *p = mutt_skip_whitespace (buf);
      if (*p == '\0') 
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

int pgp_application_pgp_handler (BODY *m, STATE *s)
{
  int could_not_decrypt = 0;
  int needpass = -1, pgp_keyblock = 0;
  int clearsign = 0, rv, rc;
  int c = 1; /* silence GCC warning */
  long bytes;
  LOFF_T last_pos, offset;
  char buf[HUGE_STRING];
  char outfile[_POSIX_PATH_MAX];
  char tmpfname[_POSIX_PATH_MAX];
  FILE *pgpout = NULL, *pgpin = NULL, *pgperr = NULL;
  FILE *tmpfp = NULL;
  pid_t thepid;

  short maybe_goodsig = 1;
  short have_any_sigs = 0;

  char *gpgcharset = NULL;
  char body_charset[STRING];
  mutt_get_body_charset (body_charset, sizeof (body_charset), m);

  rc = 0;	/* silence false compiler warning if (s->flags & M_DISPLAY) */

  fseeko (s->fpin, m->offset, 0);
  last_pos = m->offset;
  
  for (bytes = m->length; bytes > 0;)
  {
    if (fgets (buf, sizeof (buf), s->fpin) == NULL)
      break;
    
    offset = ftello (s->fpin);
    bytes -= (offset - last_pos); /* don't rely on mutt_strlen(buf) */
    last_pos = offset;
    
    if (mutt_strncmp ("-----BEGIN PGP ", buf, 15) == 0)
    {
      clearsign = 0;

      if (mutt_strcmp ("MESSAGE-----\n", buf + 15) == 0)
        needpass = 1;
      else if (mutt_strcmp ("SIGNED MESSAGE-----\n", buf + 15) == 0)
      {
	clearsign = 1;
        needpass = 0;
      }
      else if (!mutt_strcmp ("PUBLIC KEY BLOCK-----\n", buf + 15))
      {
        needpass = 0;
        pgp_keyblock = 1;
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
      mutt_mktemp (tmpfname, sizeof (tmpfname));
      if ((tmpfp = safe_fopen (tmpfname, "w+")) == NULL)
      {
	mutt_perror (tmpfname);
	return -1;
      }
      
      fputs (buf, tmpfp);
      while (bytes > 0 && fgets (buf, sizeof (buf) - 1, s->fpin) != NULL)
      {
	offset = ftello (s->fpin);
	bytes -= (offset - last_pos); /* don't rely on mutt_strlen(buf) */
	last_pos = offset;
	
	fputs (buf, tmpfp);

	if ((needpass && mutt_strcmp ("-----END PGP MESSAGE-----\n", buf) == 0) ||
	    (!needpass 
             && (mutt_strcmp ("-----END PGP SIGNATURE-----\n", buf) == 0
                 || mutt_strcmp ("-----END PGP PUBLIC KEY BLOCK-----\n",buf) == 0)))
	  break;
	/* remember optional Charset: armor header as defined by RfC4880 */
	if (mutt_strncmp ("Charset: ", buf, 9) == 0)
	{
	  size_t l = 0;
	  gpgcharset = safe_strdup (buf + 9);
	  if ((l = mutt_strlen (gpgcharset)) > 0 && gpgcharset[l-1] == '\n')
	    gpgcharset[l-1] = 0;
	  if (mutt_check_charset (gpgcharset, 0) < 0)
	    mutt_str_replace (&gpgcharset, "UTF-8");
	}
      }

      /* leave tmpfp open in case we still need it - but flush it! */
      fflush (tmpfp);

      /* Invoke PGP if needed */
      if (!clearsign || (s->flags & M_VERIFY))
      {
	mutt_mktemp (outfile, sizeof (outfile));
	if ((pgpout = safe_fopen (outfile, "w+")) == NULL)
	{
	  mutt_perror (outfile);
	  return -1;
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
            if (pgp_use_gpg_agent())
              *PgpPass = 0;
	    fprintf (pgpin, "%s\n", PgpPass);
	  }
	  
	  safe_fclose (&pgpin);

	  if (s->flags & M_DISPLAY)
	  {
	    crypt_current_time (s, "PGP");
	    rc = pgp_copy_checksig (pgperr, s->fpout);
	  }
	  
	  safe_fclose (&pgperr);
	  rv = mutt_wait_filter (thepid);
	  
	  if (s->flags & M_DISPLAY)
	  {
	    if (rc == 0) have_any_sigs = 1;
	    /*
	     * Sig is bad if
	     * gpg_good_sign-pattern did not match || pgp_decode_command returned not 0
	     * Sig _is_ correct if
	     *  gpg_good_sign="" && pgp_decode_command returned 0
	     */
	    if (rc == -1 || rv) maybe_goodsig = 0;
	    
	    state_attach_puts (_("[-- End of PGP output --]\n\n"), s);
	  }
	  if (pgp_use_gpg_agent())
	    mutt_need_hard_redraw ();
	}
	
        /* treat empty result as sign of failure */
	/* TODO: maybe on failure mutt should include the original undecoded text. */
	if (pgpout)
	{
	  rewind (pgpout);
	  c = fgetc (pgpout);
	  ungetc (c, pgpout);
	}
        if (!clearsign && (!pgpout || c == EOF))
	{
	  could_not_decrypt = 1;
	  pgp_void_passphrase ();
	}
	
	if (could_not_decrypt && !(s->flags & M_DISPLAY))
	{
          mutt_error _("Could not decrypt PGP message");
	  mutt_sleep (1);
	  rc = -1;
	  goto out;
        }
      }
      
      /*
       * Now, copy cleartext to the screen.
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
	char *expected_charset = gpgcharset && *gpgcharset ? gpgcharset : "utf-8";

	dprint(4,(debugfile,"pgp: recoding inline from [%s] to [%s]\n",
		  expected_charset, Charset));

	rewind (pgpout);
	state_set_prefix (s);
	fc = fgetconv_open (pgpout, expected_charset, Charset, M_ICONV_HOOK_FROM);
	while ((c = fgetconv (fc)) != EOF)
	  state_prefix_putc (c, s);
	fgetconv_close (&fc);
      }

      /*
       * Multiple PGP blocks can exist, so these need to be closed and
       * unlinked inside the loop.
       */
      safe_fclose (&tmpfp);
      mutt_unlink (tmpfname);
      if (pgpout)
      {
	safe_fclose (&pgpout);
	mutt_unlink (outfile);
      }

      if (s->flags & M_DISPLAY)
      {
	state_putc ('\n', s);
	if (needpass)
        {
	  state_attach_puts (_("[-- END PGP MESSAGE --]\n"), s);
	  if (could_not_decrypt)
	    mutt_error _("Could not decrypt PGP message");
	  else
	    mutt_message _("PGP message successfully decrypted.");
        }
	else if (pgp_keyblock)
	  state_attach_puts (_("[-- END PGP PUBLIC KEY BLOCK --]\n"), s);
	else
	  state_attach_puts (_("[-- END PGP SIGNED MESSAGE --]\n"), s);
      }
    }
    else
    {
      /* A traditional PGP part may mix signed and unsigned content */
      /* XXX - we may wish to recode here */
      if (s->prefix)
	state_puts (s->prefix, s);
      state_puts (buf, s);
    }
  }

  rc = 0;

out:
  m->goodsig = (maybe_goodsig && have_any_sigs);

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

  FREE(&gpgcharset);

  if (needpass == -1)
  {
    state_attach_puts (_("[-- Error: could not find beginning of PGP message! --]\n\n"), s);
    return -1;
  }
  
  return rc;
}

static int pgp_check_traditional_one_body (FILE *fp, BODY *b, int tagged_only)
{
  char tempfile[_POSIX_PATH_MAX];
  char buf[HUGE_STRING];
  FILE *tfp;
  
  short sgn = 0;
  short enc = 0;
  short key = 0;
  
  if (b->type != TYPETEXT)
    return 0;

  if (tagged_only && !b->tagged)
    return 0;

  mutt_mktemp (tempfile, sizeof (tempfile));
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
      else if (mutt_strcmp ("PUBLIC KEY BLOCK-----\n", buf + 15) == 0)
	key = 1;
    }
  }
  safe_fclose (&tfp);
  unlink (tempfile);

  if (!enc && !sgn && !key)
    return 0;

  /* fix the content type */
  
  mutt_set_parameter ("format", "fixed", &b->parameter);
  if (enc)
    mutt_set_parameter ("x-action", "pgp-encrypted", &b->parameter);
  else if (sgn)
    mutt_set_parameter ("x-action", "pgp-signed", &b->parameter);
  else if (key)
    mutt_set_parameter ("x-action", "pgp-keys", &b->parameter);
  
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
	
  fseeko (s->fpin, sigbdy->offset, 0);
  mutt_copy_bytes (s->fpin, fp, sigbdy->length);
  safe_fclose (&fp);
  
  mutt_mktemp (pgperrfile, sizeof (pgperrfile));
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

  mutt_mktemp (tempfname, sizeof (tempfname));
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
	  safe_fclose (&fpout);
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
      safe_fclose (&fpout);
      goto bailout;
    }
    mutt_copy_message (fpout, Context, h, M_CM_DECODE|M_CM_CHARCONV, 0);
  }
      
  safe_fclose (&fpout);
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

  mutt_mktemp (tempfname, sizeof (tempfname));
  if (!(tempfp = safe_fopen (tempfname, "w")))
  {
    mutt_perror (tempfname);
    return;
  }

  memset (&s, 0, sizeof (STATE));
  
  s.fpin = fp;
  s.fpout = tempfp;
  
  mutt_body_handler (top, &s);

  safe_fclose (&tempfp);

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
  int rv;
  
  mutt_mktemp (pgperrfile, sizeof (pgperrfile));
  if ((pgperr = safe_fopen (pgperrfile, "w+")) == NULL)
  {
    mutt_perror (pgperrfile);
    return NULL;
  }
  unlink (pgperrfile);

  mutt_mktemp (pgptmpfile, sizeof (pgptmpfile));
  if((pgptmp = safe_fopen (pgptmpfile, "w")) == NULL)
  {
    mutt_perror (pgptmpfile);
    safe_fclose (&pgperr);
    return NULL;
  }

  /* Position the stream at the beginning of the body, and send the data to
   * the temporary file.
   */

  fseeko (s->fpin, a->offset, 0);
  mutt_copy_bytes (s->fpin, pgptmp, a->length);
  safe_fclose (&pgptmp);

  if ((thepid = pgp_invoke_decrypt (&pgpin, &pgpout, NULL, -1, -1,
				    fileno (pgperr), pgptmpfile)) == -1)
  {
    safe_fclose (&pgperr);
    unlink (pgptmpfile);
    if (s->flags & M_DISPLAY)
      state_attach_puts (_("[-- Error: could not create a PGP subprocess! --]\n\n"), s);
    return (NULL);
  }

  /* send the PGP passphrase to the subprocess.  Never do this if the
     agent is active, because this might lead to a passphrase send as
     the message. */
  if (!pgp_use_gpg_agent())
    fputs (PgpPass, pgpin);
  fputc ('\n', pgpin);
  safe_fclose (&pgpin);
  
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

  safe_fclose (&pgpout);
  rv = mutt_wait_filter (thepid);
  mutt_unlink(pgptmpfile);
  
  if (s->flags & M_DISPLAY)
  {
    fflush (pgperr);
    rewind (pgperr);
    if (pgp_copy_checksig (pgperr, s->fpout) == 0 && !rv && p)
      p->goodsig = 1;
    else
      p->goodsig = 0;
    state_attach_puts (_("[-- End of PGP output --]\n\n"), s);
  }
  safe_fclose (&pgperr);

  fflush (fpout);
  rewind (fpout);

  if (pgp_use_gpg_agent())
    mutt_need_hard_redraw ();

  if (fgetc (fpout) == EOF)
  {
    mutt_error _("Decryption failed");
    pgp_void_passphrase ();
    return NULL;
  }

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
  int need_decode = 0;
  int saved_type;
  LOFF_T saved_offset;
  size_t saved_length;
  FILE *decoded_fp = NULL;
  int rv = 0;

  if (mutt_is_valid_multipart_pgp_encrypted (b))
    b = b->parts->next;
  else if (mutt_is_malformed_multipart_pgp_encrypted (b))
  {
    b = b->parts->next->next;
    need_decode = 1;
  }
  else
    return -1;

  memset (&s, 0, sizeof (s));
  s.fpin = fpin;

  if (need_decode)
  {
    saved_type = b->type;
    saved_offset = b->offset;
    saved_length = b->length;

    mutt_mktemp (tempfile, sizeof (tempfile));
    if ((decoded_fp = safe_fopen (tempfile, "w+")) == NULL)
    {
      mutt_perror (tempfile);
      return (-1);
    }
    unlink (tempfile);

    fseeko (s.fpin, b->offset, 0);
    s.fpout = decoded_fp;

    mutt_decode_attachment (b, &s);

    fflush (decoded_fp);
    b->length = ftello (decoded_fp);
    b->offset = 0;
    rewind (decoded_fp);
    s.fpin = decoded_fp;
    s.fpout = 0;
  }

  mutt_mktemp (tempfile, sizeof (tempfile));
  if ((*fpout = safe_fopen (tempfile, "w+")) == NULL)
  {
    mutt_perror (tempfile);
    rv = -1;
    goto bail;
  }
  unlink (tempfile);

  if ((*cur = pgp_decrypt_part (b, &s, *fpout, p)) == NULL)
    rv = -1;
  rewind (*fpout);

bail:
  if (need_decode)
  {
    b->type = saved_type;
    b->length = saved_length;
    b->offset = saved_offset;
    safe_fclose (&decoded_fp);
  }

  return rv;
}

/*
 * This handler is passed the application/octet-stream directly.
 * The caller must propagate a->goodsig to its parent.
 */
int pgp_encrypted_handler (BODY *a, STATE *s)
{
  char tempfile[_POSIX_PATH_MAX];
  FILE *fpout, *fpin;
  BODY *tattach;
  int rc = 0;

  mutt_mktemp (tempfile, sizeof (tempfile));
  if ((fpout = safe_fopen (tempfile, "w+")) == NULL)
  {
    if (s->flags & M_DISPLAY)
      state_attach_puts (_("[-- Error: could not create temporary file! --]\n"), s);
    return -1;
  }

  if (s->flags & M_DISPLAY) crypt_current_time (s, "PGP");

  if ((tattach = pgp_decrypt_part (a, s, fpout, a)) != NULL)
  {
    if (s->flags & M_DISPLAY)
      state_attach_puts (_("[-- The following data is PGP/MIME encrypted --]\n\n"), s);

    fpin = s->fpin;
    s->fpin = fpout;
    rc = mutt_body_handler (tattach, s);
    s->fpin = fpin;

    /* 
     * if a multipart/signed is the _only_ sub-part of a
     * multipart/encrypted, cache signature verification
     * status.
     *
     */
    
    if (mutt_is_multipart_signed (tattach) && !tattach->next)
      a->goodsig |= tattach->goodsig;
    
    if (s->flags & M_DISPLAY)
    {
      state_puts ("\n", s);
      state_attach_puts (_("[-- End of PGP/MIME encrypted data --]\n"), s);
    }

    mutt_free_body (&tattach);
    /* clear 'Invoking...' message, since there's no error */
    mutt_message _("PGP message successfully decrypted.");
  }
  else
  {
    mutt_error _("Could not decrypt PGP message");
    /* void the passphrase, even if it's not necessarily the problem */
    pgp_void_passphrase ();
    rc = -1;
  }

  safe_fclose (&fpout);
  mutt_unlink(tempfile);

  return rc;
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

  mutt_mktemp (sigfile, sizeof (sigfile));
  if ((fp = safe_fopen (sigfile, "w")) == NULL)
  {
    return (NULL);
  }

  mutt_mktemp (signedfile, sizeof (signedfile));
  if ((sfp = safe_fopen(signedfile, "w")) == NULL)
  {
    mutt_perror(signedfile);
    safe_fclose (&fp);
    unlink(sigfile);
    return NULL;
  }
  
  mutt_write_mime_header (a, sfp);
  fputc ('\n', sfp);
  mutt_write_mime_body (a, sfp);
  safe_fclose (&sfp);
  
  if ((thepid = pgp_invoke_sign (&pgpin, &pgpout, &pgperr,
				 -1, -1, -1, signedfile)) == -1)
  {
    mutt_perror _("Can't open PGP subprocess!");
    safe_fclose (&fp);
    unlink(sigfile);
    unlink(signedfile);
    return NULL;
  }
  
  if (!pgp_use_gpg_agent())
     fputs(PgpPass, pgpin);
  fputc('\n', pgpin);
  safe_fclose (&pgpin);
  
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

  if(mutt_wait_filter (thepid) && option(OPTPGPCHECKEXIT))
    empty=1;

  safe_fclose (&pgperr);
  safe_fclose (&pgpout);
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
    /* most likely error is a bad passphrase, so automatically forget it */
    pgp_void_passphrase ();
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
  t->disposition = DISPNONE;
  t->encoding = ENC7BIT;
  t->unlink = 1; /* ok to remove this file after sending. */
  mutt_set_parameter ("name", "signature.asc", &t->parameter);

  return (a);
}

/* This routine attempts to find the keyids of the recipients of a message.
 * It returns NULL if any of the keys can not be found.
 * If oppenc_mode is true, only keys that can be determined without
 * prompting will be used.
 */
char *pgp_findKeys (ADDRESS *adrlist, int oppenc_mode)
{
  LIST *crypt_hook_list, *crypt_hook = NULL;
  char *keyID, *keylist = NULL;
  size_t keylist_size = 0;
  size_t keylist_used = 0;
  ADDRESS *addr = NULL;
  ADDRESS *p, *q;
  pgp_key_t k_info = NULL;
  char buf[LONG_STRING];
  int r;
  int key_selected;

  const char *fqdn = mutt_fqdn (1);

  for (p = adrlist; p ; p = p->next)
  {
    key_selected = 0;
    crypt_hook_list = crypt_hook = mutt_crypt_hook (p);
    do
    {
      q = p;
      k_info = NULL;

      if (crypt_hook != NULL)
      {
        keyID = crypt_hook->data;
        r = M_YES;
        if (! oppenc_mode && option(OPTCRYPTCONFIRMHOOK))
        {
          snprintf (buf, sizeof (buf), _("Use keyID = \"%s\" for %s?"), keyID, p->mailbox);
          r = mutt_yesorno (buf, M_YES);
        }
        if (r == M_YES)
        {
          if (crypt_is_numerical_keyid (keyID))
          {
            if (strncmp (keyID, "0x", 2) == 0)
              keyID += 2;
            goto bypass_selection;		/* you don't see this. */
          }

          /* check for e-mail address */
          if (strchr (keyID, '@') &&
              (addr = rfc822_parse_adrlist (NULL, keyID)))
          {
            if (fqdn) rfc822_qualify (addr, fqdn);
            q = addr;
          }
          else if (! oppenc_mode)
          {
            k_info = pgp_getkeybystr (keyID, KEYFLAG_CANENCRYPT, PGP_PUBRING);
          }
        }
        else if (r == M_NO)
        {
          if (key_selected || (crypt_hook->next != NULL))
          {
            crypt_hook = crypt_hook->next;
            continue;
          }
        }
        else if (r == -1)
        {
          FREE (&keylist);
          rfc822_free_address (&addr);
          mutt_free_list (&crypt_hook_list);
          return NULL;
        }
      }

      if (k_info == NULL)
      {
        pgp_invoke_getkeys (q);
        k_info = pgp_getkeybyaddr (q, KEYFLAG_CANENCRYPT, PGP_PUBRING, oppenc_mode);
      }

      if ((k_info == NULL) && (! oppenc_mode))
      {
        snprintf (buf, sizeof (buf), _("Enter keyID for %s: "), q->mailbox);
        k_info = pgp_ask_for_key (buf, q->mailbox,
                              KEYFLAG_CANENCRYPT, PGP_PUBRING);
      }

      if (k_info == NULL)
      {
        FREE (&keylist);
        rfc822_free_address (&addr);
        mutt_free_list (&crypt_hook_list);
        return NULL;
      }

      keyID = pgp_fpr_or_lkeyid (k_info);

    bypass_selection:
      keylist_size += mutt_strlen (keyID) + 4;
      safe_realloc (&keylist, keylist_size);
      sprintf (keylist + keylist_used, "%s0x%s", keylist_used ? " " : "",	/* __SPRINTF_CHECKED__ */
              keyID);
      keylist_used = mutt_strlen (keylist);

      key_selected = 1;

      pgp_free_key (&k_info);
      rfc822_free_address (&addr);

      if (crypt_hook != NULL)
        crypt_hook = crypt_hook->next;

    } while (crypt_hook != NULL);

    mutt_free_list (&crypt_hook_list);
  }
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
  int empty = 0;
  pid_t thepid;
  
  mutt_mktemp (tempfile, sizeof (tempfile));
  if ((fpout = safe_fopen (tempfile, "w+")) == NULL)
  {
    mutt_perror (tempfile);
    return (NULL);
  }

  mutt_mktemp (pgperrfile, sizeof (pgperrfile));
  if ((pgperr = safe_fopen (pgperrfile, "w+")) == NULL)
  {
    mutt_perror (pgperrfile);
    unlink(tempfile);
    safe_fclose (&fpout);
    return NULL;
  }
  unlink (pgperrfile);

  mutt_mktemp (pgpinfile, sizeof (pgpinfile));
  if((fptmp = safe_fopen(pgpinfile, "w")) == NULL)
  {
    mutt_perror(pgpinfile);
    unlink(tempfile);
    safe_fclose (&fpout);
    safe_fclose (&pgperr);
    return NULL;
  }
  
  if (sign)
    convert_to_7bit (a);
  
  mutt_write_mime_header (a, fptmp);
  fputc ('\n', fptmp);
  mutt_write_mime_body (a, fptmp);
  safe_fclose (&fptmp);
  
  if ((thepid = pgp_invoke_encrypt (&pgpin, NULL, NULL, -1, 
				    fileno (fpout), fileno (pgperr),
				    pgpinfile, keylist, sign)) == -1)
  {
    safe_fclose (&pgperr);
    unlink(pgpinfile);
    return (NULL);
  }

  if (sign)
  {
    if (!pgp_use_gpg_agent())
       fputs (PgpPass, pgpin);
    fputc ('\n', pgpin);
  }
  safe_fclose (&pgpin);
  
  if(mutt_wait_filter (thepid) && option(OPTPGPCHECKEXIT))
    empty=1;

  unlink(pgpinfile);
  
  fflush (fpout);
  rewind (fpout);
  if(!empty)
    empty = (fgetc (fpout) == EOF);
  safe_fclose (&fpout);

  fflush (pgperr);
  rewind (pgperr);
  while (fgets (buf, sizeof (buf) - 1, pgperr) != NULL)
  {
    err = 1;
    fputs (buf, stdout);
  }
  safe_fclose (&pgperr);

  /* pause if there is any error output from PGP */
  if (err)
    mutt_any_key_to_continue (NULL);

  if (empty)
  {
    /* fatal error while trying to encrypt message */
    if (sign)
      pgp_void_passphrase (); /* just in case */
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
  t->parts->next->disposition = DISPATTACH;
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

  int empty = 0;
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
  
  mutt_mktemp (pgpinfile, sizeof (pgpinfile));
  if ((pgpin = safe_fopen (pgpinfile, "w")) == NULL)
  {
    mutt_perror (pgpinfile);
    safe_fclose (&fp);
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

    /* fromcode is assumed to be correct: we set flags to 0 */
    fc = fgetconv_open (fp, from_charset, "utf-8", 0);
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
  safe_fclose (&pgpin);

  mutt_mktemp (pgpoutfile, sizeof (pgpoutfile));
  mutt_mktemp (pgperrfile, sizeof (pgperrfile));
  if ((pgpout = safe_fopen (pgpoutfile, "w+")) == NULL ||
      (pgperr = safe_fopen (pgperrfile, "w+")) == NULL)
  {
    mutt_perror (pgpout ? pgperrfile : pgpoutfile);
    unlink (pgpinfile);
    if (pgpout) 
    {
      safe_fclose (&pgpout);
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
    safe_fclose (&pgpout);
    safe_fclose (&pgperr);
    mutt_unlink (pgpinfile);
    unlink (pgpoutfile);
    return NULL;
  }

  if (pgp_use_gpg_agent())
    *PgpPass = 0;
  if (flags & SIGN)
    fprintf (pgpin, "%s\n", PgpPass);
  safe_fclose (&pgpin);

  if(mutt_wait_filter (thepid) && option(OPTPGPCHECKEXIT))
    empty=1;

  mutt_unlink (pgpinfile);

  fflush (pgpout);
  fflush (pgperr);

  rewind (pgpout);
  rewind (pgperr);
  
  if(!empty)
    empty = (fgetc (pgpout) == EOF);
  safe_fclose (&pgpout);
  
  err = 0;
  
  while (fgets (buff, sizeof (buff), pgperr))
  {
    err = 1;
    fputs (buff, stdout);
  }
  
  safe_fclose (&pgperr);
  
  if (err)
    mutt_any_key_to_continue (NULL);
  
  if (empty)
  {
    if (flags & SIGN)
      pgp_void_passphrase (); /* just in case */
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

  b->disposition = DISPNONE;
  b->unlink   = 1;

  b->noconv = 1;
  b->use_disp = 0;
  
  if (!(flags & ENCRYPT))
    b->encoding = a->encoding;
  
  return b;
}

int pgp_send_menu (HEADER *msg, int *redraw)
{
  pgp_key_t p;
  char input_signas[SHORT_STRING];
  char *prompt, *letters, *choices;
  char promptbuf[LONG_STRING];
  int choice;

  if (!(WithCrypto & APPLICATION_PGP))
    return msg->security;

  /* If autoinline and no crypto options set, then set inline. */
  if (option (OPTPGPAUTOINLINE) && 
      !((msg->security & APPLICATION_PGP) && (msg->security & (SIGN|ENCRYPT))))
    msg->security |= INLINE;

  msg->security |= APPLICATION_PGP;

  /*
   * Opportunistic encrypt is controlling encryption.  Allow to toggle
   * between inline and mime, but not turn encryption on or off.
   * NOTE: "Signing" and "Clearing" only adjust the sign bit, so we have different
   *       letter choices for those.
   */
  if (option (OPTCRYPTOPPORTUNISTICENCRYPT) && (msg->security & OPPENCRYPT))
  {
    if (msg->security & (ENCRYPT | SIGN))
    {
      snprintf (promptbuf, sizeof (promptbuf),
          _("PGP (s)ign, sign (a)s, %s format, (c)lear, or (o)ppenc mode off? "),
          (msg->security & INLINE) ? _("PGP/M(i)ME") : _("(i)nline"));
      prompt = promptbuf;
      letters = _("safcoi");
      choices = "SaFCoi";
    }
    else
    {
      prompt = _("PGP (s)ign, sign (a)s, (c)lear, or (o)ppenc mode off? ");
      letters = _("safco");
      choices = "SaFCo";
    }
  }
  /*
   * Opportunistic encryption option is set, but is toggled off
   * for this message.
   */
  else if (option (OPTCRYPTOPPORTUNISTICENCRYPT))
  {
    /* When the message is not selected for signing or encryption, the toggle
    * between PGP/MIME and Traditional doesn't make sense.
    */
    if (msg->security & (ENCRYPT | SIGN))
    {

      snprintf (promptbuf, sizeof (promptbuf), 
          _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, %s format, (c)lear, or (o)ppenc mode? "),
          (msg->security & INLINE) ? _("PGP/M(i)ME") : _("(i)nline"));
      prompt = promptbuf;
      letters = _("esabfcoi");
      choices = "esabfcOi";
    }
    else
    {
      prompt = _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (c)lear, or (o)ppenc mode? ");
      letters = _("esabfco");
      choices = "esabfcO";
    }
  }
  /*
   * Opportunistic encryption is unset
   */
  else
  {
    if (msg->security & (ENCRYPT | SIGN))
    {

      snprintf (promptbuf, sizeof (promptbuf), 
          _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, %s format, or (c)lear? "),
          (msg->security & INLINE) ? _("PGP/M(i)ME") : _("(i)nline"));
      prompt = promptbuf;
      letters = _("esabfci");
      choices = "esabfci";
    }
    else
    {
      prompt = _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (c)lear? ");
      letters = _("esabfc");
      choices = "esabfc";
    }
  }

  choice = mutt_multi_choice (prompt, letters);
  if (choice > 0)
  {
    switch (choices[choice - 1])
    {
    case 'e': /* (e)ncrypt */
      msg->security |= ENCRYPT;
      msg->security &= ~SIGN;
      break;

    case 's': /* (s)ign */
      msg->security &= ~ENCRYPT;
      msg->security |= SIGN;
      break;

    case 'S': /* (s)ign in oppenc mode */
      msg->security |= SIGN;
      break;

    case 'a': /* sign (a)s */
      unset_option(OPTPGPCHECKTRUST);

      if ((p = pgp_ask_for_key (_("Sign as: "), NULL, 0, PGP_SECRING)))
      {
        snprintf (input_signas, sizeof (input_signas), "0x%s",
            pgp_fpr_or_lkeyid (p));
        mutt_str_replace (&PgpSignAs, input_signas);
        pgp_free_key (&p);

        msg->security |= SIGN;

        crypt_pgp_void_passphrase ();  /* probably need a different passphrase */
      }
      *redraw = REDRAW_FULL;
      break;

    case 'b': /* (b)oth */
      msg->security |= (ENCRYPT | SIGN);
      break;

    case 'f': /* (f)orget it: kept for backward compatibility. */
    case 'c': /* (c)lear     */
      msg->security &= ~(ENCRYPT | SIGN);
      break;

    case 'F': /* (f)orget it or (c)lear in oppenc mode */
    case 'C':
      msg->security &= ~SIGN;
      break;

    case 'O': /* oppenc mode on */
      msg->security |= OPPENCRYPT;
      crypt_opportunistic_encrypt (msg);
      break;

    case 'o': /* oppenc mode off */
      msg->security &= ~OPPENCRYPT;
      break;

    case 'i': /* toggle (i)nline */
      msg->security ^= INLINE;
      break;
    }
  }

  return (msg->security);
}


#endif /* CRYPT_BACKEND_CLASSIC_PGP */
