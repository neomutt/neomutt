/*
 * Copyright (C) 1996,1997 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2001  Thomas Roessler <roessler@does-not-exist.org>
 *                     Oliver Ehli <elmy@acm.org>
 * Copyright (C) 2003  Werner Koch <wk@gnupg.org>
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


#include "mutt.h"
#include "mutt_curses.h"
#include "mime.h"
#include "copy.h"
#include "mutt_crypt.h"

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


/* print the current time to avoid spoofing of the signature output */
void crypt_current_time(STATE *s, char *app_name)
{
  time_t t;
  char p[STRING], tmp[STRING];

  if (!WithCrypto)
    return;

  if (option (OPTCRYPTTIMESTAMP))
  {
    t = time(NULL);
    setlocale (LC_TIME, "");
    strftime (p, sizeof (p), _(" (current time: %c)"), localtime (&t));
    setlocale (LC_TIME, "C");
  }
  else
    *p = '\0';

  snprintf (tmp, sizeof (tmp), _("[-- %s output follows%s --]\n"), NONULL(app_name), p);
  state_attach_puts (tmp, s);
}



void crypt_forget_passphrase (void)
{
  if ((WithCrypto & APPLICATION_PGP))
    crypt_pgp_void_passphrase ();

  if ((WithCrypto & APPLICATION_SMIME))
    crypt_smime_void_passphrase ();

  if (WithCrypto)
    mutt_message _("Passphrase(s) forgotten.");
}


#if defined(HAVE_SETRLIMIT) && (!defined(DEBUG))

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

#endif /* HAVE_SETRLIMIT */


int crypt_valid_passphrase(int flags)
{
  time_t now = time (NULL);

# if defined(HAVE_SETRLIMIT) &&(!defined(DEBUG))
  disable_coredumps ();
# endif

  if ((WithCrypto & APPLICATION_PGP) && (flags & APPLICATION_PGP))
  {
    extern char PgpPass[STRING];
    extern time_t PgpExptime;

    if (pgp_use_gpg_agent())
    {
      *PgpPass = 0;
      return 0; /* handled by gpg-agent */
    }

    if (now < PgpExptime) return 1; /* just use the cached copy. */
    crypt_pgp_void_passphrase ();
      
    if (mutt_get_password (_("Enter PGP passphrase:"),
                           PgpPass, sizeof (PgpPass)) == 0)
    {
      PgpExptime = time (NULL) + PgpTimeout;
      return (1);
    }
    else
      PgpExptime = 0;
    }

  if ((WithCrypto & APPLICATION_SMIME) && (flags & APPLICATION_SMIME))
  {
    extern char SmimePass[STRING];
    extern time_t SmimeExptime;

    if (now < SmimeExptime) return (1);
    crypt_smime_void_passphrase ();
      
    if (mutt_get_password (_("Enter SMIME passphrase:"), SmimePass,
			   sizeof (SmimePass)) == 0)
    {
      SmimeExptime = time (NULL) + SmimeTimeout;
      return (1);
    }
    else
      SmimeExptime = 0;
  }

  return (0);
}



int mutt_protect (HEADER *msg, HEADER *cur, char *keylist)
{
  BODY *pbody = NULL, *tmp_pbody = NULL;
  BODY *tmp_smime_pbody = NULL;
  BODY *tmp_pgp_pbody = NULL;
  int traditional = 0;
  int flags = (WithCrypto & APPLICATION_PGP)? msg->security: 0;
  int i;

  if (!WithCrypto)
    return -1;

  if ((msg->security & SIGN) && !crypt_valid_passphrase (msg->security))
    return (-1);

  if ((WithCrypto & APPLICATION_PGP) && (msg->security & APPLICATION_PGP))
  {
    if ((msg->content->type == TYPETEXT) &&
	!ascii_strcasecmp (msg->content->subtype, "plain"))
    {
      if (cur && cur->security && option (OPTPGPAUTOTRAD)
	  && (option (OPTCRYPTREPLYENCRYPT)
	      || option (OPTCRYPTREPLYSIGN)
	      || option (OPTCRYPTREPLYSIGNENCRYPTED)))
	{
	  if(mutt_is_application_pgp(cur->content))
	    traditional = 1;
	}
      else
	{
	  if ((i = query_quadoption (OPT_PGPTRADITIONAL, _("Create a traditional (inline) PGP message?"))) == -1)
	    return -1;
	  else if (i == M_YES)
	    traditional = 1;
	}
    }
    if (traditional)
    {
      if (!isendwin ()) mutt_endwin _("Invoking PGP...");
      if (!(pbody = crypt_pgp_traditional_encryptsign (msg->content, flags, keylist)))
	return -1;

      msg->content = pbody;
      return 0;
    }
  }

  if (!isendwin ()) mutt_endwin (NULL);

  if ((WithCrypto & APPLICATION_SMIME))
    tmp_smime_pbody = msg->content;

  if (msg->security & SIGN)
  {
    if ((WithCrypto & APPLICATION_SMIME)
        && (msg->security & APPLICATION_SMIME))
    {
      if (!(tmp_pbody = crypt_smime_sign_message (msg->content)))
	return -1;
      pbody = tmp_smime_pbody = tmp_pbody;
    }

    if ((WithCrypto & APPLICATION_PGP)
        && (msg->security & APPLICATION_PGP)
        && (!(flags & ENCRYPT) || option (OPTPGPRETAINABLESIG)))
    {
      if (!(tmp_pbody = crypt_pgp_sign_message (msg->content)))
        return -1;

      flags &= ~SIGN;
      pbody = tmp_pgp_pbody = tmp_pbody;
    }

    if (WithCrypto
        && (msg->security & APPLICATION_SMIME)
	&& (msg->security & APPLICATION_PGP))
    {
	/* here comes the draft ;-) */
    }
  }


  if (msg->security & ENCRYPT)
  {
    if ((WithCrypto & APPLICATION_SMIME)
        && (msg->security & APPLICATION_SMIME))
    {
      if (!(tmp_pbody = crypt_smime_build_smime_entity (tmp_smime_pbody,
                                                        keylist)))
      {
	/* signed ? free it! */
	return (-1);
      }
      /* free tmp_body if messages was signed AND encrypted ... */
      if (tmp_smime_pbody != msg->content && tmp_smime_pbody != tmp_pbody)
      {
	/* detatch and dont't delete msg->content,
	   which tmp_smime_pbody->parts after signing. */
	tmp_smime_pbody->parts = tmp_smime_pbody->parts->next;
	msg->content->next = NULL;
	mutt_free_body (&tmp_smime_pbody);
      }
      pbody = tmp_pbody;
    }

    if ((WithCrypto & APPLICATION_PGP)
        && (msg->security & APPLICATION_PGP))
    {
      if (!(pbody = crypt_pgp_encrypt_message (msg->content, keylist,
                                               flags & SIGN)))
      {

	/* did we perform a retainable signature? */
	if (flags != msg->security)
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
      if (flags != msg->security)
      {
	mutt_remove_multipart (msg->content);
	mutt_free_body (&msg->content->next);
      }
    }
  }

  if(pbody)
      msg->content = pbody;

  return 0;
}


   
     
int mutt_is_multipart_signed (BODY *b)
{
  char *p;

  if (!b || !(b->type == TYPEMULTIPART) ||
      !b->subtype || ascii_strcasecmp(b->subtype, "signed"))
    return 0;

  if (!(p = mutt_get_parameter("protocol", b->parameter)))
    return 0;

  if (!(ascii_strcasecmp (p, "multipart/mixed")))
    return SIGN;

  if ((WithCrypto & APPLICATION_PGP)
      && !(ascii_strcasecmp (p, "application/pgp-signature")))
    return PGPSIGN;
    
  if ((WithCrypto & APPLICATION_SMIME)
      && !(ascii_strcasecmp (p, "application/x-pkcs7-signature")))
    return SMIMESIGN;
  if ((WithCrypto & APPLICATION_SMIME)
      && !(ascii_strcasecmp (p, "application/pkcs7-signature")))
    return SMIMESIGN;

  return 0;
}
   
     
int mutt_is_multipart_encrypted (BODY *b)
{
  if ((WithCrypto & APPLICATION_PGP))
  {
    char *p;
  
    if (!b || b->type != TYPEMULTIPART ||
        !b->subtype || ascii_strcasecmp (b->subtype, "encrypted") ||
        !(p = mutt_get_parameter ("protocol", b->parameter)) ||
        ascii_strcasecmp (p, "application/pgp-encrypted"))
      return 0;
  
     return PGPENCRYPT;
  }

  return 0;
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
    else if (p && !ascii_strncasecmp ("pgp-keys", p, 7))
      t |= PGPKEY;
  }
  return t;
}

int mutt_is_application_smime (BODY *m)
{
  char *t=NULL;
  int len, complain=0;

  if ((m->type & TYPEAPPLICATION) && m->subtype)
  {
    /* S/MIME MIME types don't need x- anymore, see RFC2311 */
    if (!ascii_strcasecmp (m->subtype, "x-pkcs7-mime") ||
	!ascii_strcasecmp (m->subtype, "pkcs7-mime"))
    {
      if ((t = mutt_get_parameter ("smime-type", m->parameter)))
      {
	if (!ascii_strcasecmp (t, "enveloped-data"))
	  return SMIMEENCRYPT;
	else if (!ascii_strcasecmp (t, "signed-data"))
	  return (SMIMESIGN|SMIMEOPAQUE);
	else return 0;
      }
      /* Netscape 4.7 uses 
       * Content-Description: S/MIME Encrypted Message
       * instead of Content-Type parameter
       */
      if (!ascii_strcasecmp (m->description, "S/MIME Encrypted Message"))
	return SMIMEENCRYPT;
      complain = 1;
    }
    else if (ascii_strcasecmp (m->subtype, "octet-stream"))
      return 0;

    t = mutt_get_parameter ("name", m->parameter);

    if (!t) t = m->d_filename;
    if (!t) t = m->filename;
    if (!t) 
    {
      if (complain)
	mutt_message (_("S/MIME messages with no hints on content are unsupported."));
      return 0;
    }

    /* no .p7c, .p10 support yet. */

    len = mutt_strlen (t) - 4;
    if (len > 0 && *(t+len) == '.')
    {
      len++;
      if (!ascii_strcasecmp ((t+len), "p7m"))
#if 0
       return SMIMEENCRYPT;
#else
      /* Not sure if this is the correct thing to do, but 
         it's required for compatibility with Outlook */
       return (SMIMESIGN|SMIMEOPAQUE);
#endif
      else if (!ascii_strcasecmp ((t+len), "p7s"))
	return (SMIMESIGN|SMIMEOPAQUE);
    }
  }

  return 0;
}






int crypt_query (BODY *m)
{
  int t = 0;

  if (!WithCrypto)
    return 0;

  if (m->type == TYPEAPPLICATION)
  {
    if ((WithCrypto & APPLICATION_PGP))
      t |= mutt_is_application_pgp(m);

    if ((WithCrypto & APPLICATION_SMIME))
    {
      t |= mutt_is_application_smime(m);
      if (t && m->goodsig) t |= GOODSIGN;
      if (t && m->badsig) t |= BADSIGN;
    }
  }
  else if ((WithCrypto & APPLICATION_PGP) && m->type == TYPETEXT)
  {
    t |= mutt_is_application_pgp (m);
    if (t && m->goodsig)
      t |= GOODSIGN;
  }
  
  if (m->type == TYPEMULTIPART)
  {
    t |= mutt_is_multipart_encrypted(m);
    t |= mutt_is_multipart_signed (m);

    if (t && m->goodsig) t |= GOODSIGN;
  }

  if (m->type == TYPEMULTIPART || m->type == TYPEMESSAGE)
  {
    BODY *p;
 
    for (p = m->parts; p; p = p->next)
      t |= crypt_query (p) & ~GOODSIGN;
  }

  return t;
}




int crypt_write_signed(BODY *a, STATE *s, const char *tempfile)
{
  FILE *fp;
  int c;
  short hadcr;
  size_t bytes;

  if (!WithCrypto)
    return -1;

  if (!(fp = safe_fopen (tempfile, "w")))
  {
    mutt_perror (tempfile);
    return -1;
  }
      
  fseek (s->fpin, a->hdr_offset, 0);
  bytes = a->length + a->offset - a->hdr_offset;
  hadcr = 0;
  while (bytes > 0)
  {
    if ((c = fgetc (s->fpin)) == EOF)
      break;
    
    bytes--;
    
    if  (c == '\r')
      hadcr = 1;
    else 
    {
      if (c == '\n' && !hadcr)
	fputc ('\r', fp);
      
      hadcr = 0;
    }
    
    fputc (c, fp);
    
  }
  fclose (fp);

  return 0;
}



void convert_to_7bit (BODY *a)
{
  if (!WithCrypto)
    return;

  while (a)
  {
    if (a->type == TYPEMULTIPART)
    {
      if (a->encoding != ENC7BIT)
      {
        a->encoding = ENC7BIT;
	convert_to_7bit(a->parts);
      }
      else if ((WithCrypto & APPLICATION_PGP) && option (OPTPGPSTRICTENC))
	convert_to_7bit (a->parts);
    } 
    else if (a->type == TYPEMESSAGE &&
	     mutt_strcasecmp(a->subtype, "delivery-status"))
    {
      if(a->encoding != ENC7BIT)
	mutt_message_to_7bit (a, NULL);
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




void crypt_extract_keys_from_messages (HEADER * h)
{
  int i;
  char tempfname[_POSIX_PATH_MAX], *mbox;
  ADDRESS *tmp = NULL;
  FILE *fpout;

  if (!WithCrypto)
    return;

  mutt_mktemp (tempfname);
  if (!(fpout = safe_fopen (tempfname, "w")))
  {
    mutt_perror (tempfname);
    return;
  }

  if ((WithCrypto & APPLICATION_PGP))
    set_option (OPTDONTHANDLEPGPKEYS);

  if (!h)
  {
    for (i = 0; i < Context->vcount; i++)
    {
      if (Context->hdrs[Context->v2r[i]]->tagged)
      {
	mutt_parse_mime_message (Context, Context->hdrs[Context->v2r[i]]);
	if (Context->hdrs[Context->v2r[i]]->security & ENCRYPT &&
	    !crypt_valid_passphrase (Context->hdrs[Context->v2r[i]]->security))
	{
	  fclose (fpout);
	  break;
	}

	if ((WithCrypto & APPLICATION_PGP)
            && (Context->hdrs[Context->v2r[i]]->security & APPLICATION_PGP))
	{
	  mutt_copy_message (fpout, Context, Context->hdrs[Context->v2r[i]], 
			     M_CM_DECODE|M_CM_CHARCONV, 0);
	  fflush(fpout);
	  
	  mutt_endwin (_("Trying to extract PGP keys...\n"));
	  crypt_pgp_invoke_import (tempfname);
	}

	if ((WithCrypto & APPLICATION_SMIME)
            && (Context->hdrs[Context->v2r[i]]->security & APPLICATION_SMIME))
	{
	  if (Context->hdrs[Context->v2r[i]]->security & ENCRYPT)
	    mutt_copy_message (fpout, Context, Context->hdrs[Context->v2r[i]],
			       M_CM_NOHEADER|M_CM_DECODE_CRYPT
                               |M_CM_DECODE_SMIME, 0);
	  else
	    mutt_copy_message (fpout, Context,
			       Context->hdrs[Context->v2r[i]], 0, 0);
	  fflush(fpout);

          if (Context->hdrs[Context->v2r[i]]->env->from)
	    tmp = mutt_expand_aliases (h->env->from);
	  else if (Context->hdrs[Context->v2r[i]]->env->sender)
	    tmp = mutt_expand_aliases (Context->hdrs[Context->v2r[i]]
                                                    ->env->sender);
          mbox = tmp ? tmp->mailbox : NULL;
	  if (mbox)
	  {
	    mutt_endwin (_("Trying to extract S/MIME certificates...\n"));
	    crypt_smime_invoke_import (tempfname, mbox);
	    tmp = NULL;
	  }
	}

	rewind (fpout);
      }
    }
  }
  else
  {
    mutt_parse_mime_message (Context, h);
    if (!(h->security & ENCRYPT && !crypt_valid_passphrase (h->security)))
    {
      if ((WithCrypto & APPLICATION_PGP)
          && (h->security & APPLICATION_PGP))
      {
	mutt_copy_message (fpout, Context, h, M_CM_DECODE|M_CM_CHARCONV, 0);
	fflush(fpout);
	mutt_endwin (_("Trying to extract PGP keys...\n"));
	crypt_pgp_invoke_import (tempfname);
      }

      if ((WithCrypto & APPLICATION_SMIME)
          && (h->security & APPLICATION_SMIME))
      {
	if (h->security & ENCRYPT)
	  mutt_copy_message (fpout, Context, h, M_CM_NOHEADER
                                                |M_CM_DECODE_CRYPT
                                                |M_CM_DECODE_SMIME, 0);
	else
	  mutt_copy_message (fpout, Context, h, 0, 0);

	fflush(fpout);
	if (h->env->from) tmp = mutt_expand_aliases (h->env->from);
	else if (h->env->sender)  tmp = mutt_expand_aliases (h->env->sender); 
	mbox = tmp ? tmp->mailbox : NULL;
	if (mbox) /* else ? */
	{
	  mutt_message (_("Trying to extract S/MIME certificates...\n"));
	  crypt_smime_invoke_import (tempfname, mbox);
	}
      }
    }
  }
      
  fclose (fpout);
  if (isendwin())
    mutt_any_key_to_continue (NULL);

  mutt_unlink (tempfname);

  if ((WithCrypto & APPLICATION_PGP))
    unset_option (OPTDONTHANDLEPGPKEYS);
}



int crypt_get_keys (HEADER *msg, char **keylist)
{
  /* Do a quick check to make sure that we can find all of the encryption
   * keys if the user has requested this service.
   */

  if (!WithCrypto)
    return 0;

  if ((WithCrypto & APPLICATION_PGP))
    set_option (OPTPGPCHECKTRUST);

  *keylist = NULL;

  if (msg->security & ENCRYPT)
  {
     if ((WithCrypto & APPLICATION_PGP)
         && (msg->security & APPLICATION_PGP))
     {
       if ((*keylist = crypt_pgp_findkeys (msg->env->to, msg->env->cc,
      			       msg->env->bcc)) == NULL)
           return (-1);
       unset_option (OPTPGPCHECKTRUST);
     }
     if ((WithCrypto & APPLICATION_SMIME)
         && (msg->security & APPLICATION_SMIME))
     {
       if ((*keylist = crypt_smime_findkeys (msg->env->to, msg->env->cc,
      				             msg->env->bcc)) == NULL)
           return (-1);
     }
  }
    
  return (0);
}



static void crypt_fetch_signatures (BODY ***signatures, BODY *a, int *n)
{
  if (!WithCrypto)
    return;

  for (; a; a = a->next)
  {
    if (a->type == TYPEMULTIPART)
      crypt_fetch_signatures (signatures, a->parts, n);
    else
    {
      if((*n % 5) == 0)
	safe_realloc ((void **) signatures, (*n + 6) * sizeof (BODY **));

      (*signatures)[(*n)++] = a;
    }
  }
}


/*
 * This routine verifies a  "multipart/signed"  body.
 */

void mutt_signed_handler (BODY *a, STATE *s)
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

  if (!WithCrypto)
    return;

  protocol = mutt_get_parameter ("protocol", a->parameter);
  a = a->parts;

  /* extract the protocol information */
  
  if (protocol)
  {
    char major[STRING];
    char *t;

    if ((protocol_minor = strchr (protocol, '/'))) protocol_minor++;
    
    strfcpy (major, protocol, sizeof(major));
    if((t = strchr(major, '/')))
      *t = '\0';
    
    protocol_major = mutt_check_mime_type (major);
  }

  /* consistency check */

  if (!(a && a->next && a->next->type == protocol_major && 
      !mutt_strcasecmp (a->next->subtype, protocol_minor)))
  {
    state_attach_puts (_("[-- Error: "
                         "Inconsistent multipart/signed structure! --]\n\n"),
                       s);
    mutt_body_handler (a, s);
    return;
  }

  
  if ((WithCrypto & APPLICATION_PGP)
      && protocol_major == TYPEAPPLICATION
      && !mutt_strcasecmp (protocol_minor, "pgp-signature"))
    ;
  else if ((WithCrypto & APPLICATION_SMIME)
           && protocol_major == TYPEAPPLICATION
	   && !(mutt_strcasecmp (protocol_minor, "x-pkcs7-signature")
	       && mutt_strcasecmp (protocol_minor, "pkcs7-signature"))
    ;
  else if (protocol_major == TYPEMULTIPART
	   && !mutt_strcasecmp (protocol_minor, "mixed"))
    ;
  else
  {
    state_printf (s, _("[-- Error: "
                       "Unknown multipart/signed protocol %s! --]\n\n"),
                  protocol);
    mutt_body_handler (a, s);
    return;
  }
  
  if (s->flags & M_DISPLAY)
  {
    
    crypt_fetch_signatures (&signatures, a->next, &sigcnt);
    
    if (sigcnt)
    {
      mutt_mktemp (tempfile);
      if (crypt_write_signed (a, s, tempfile) == 0)
      {
	for (i = 0; i < sigcnt; i++)
	{
	  if ((WithCrypto & APPLICATION_PGP)
              && signatures[i]->type == TYPEAPPLICATION 
	      && !mutt_strcasecmp (signatures[i]->subtype, "pgp-signature"))
	  {
	    if (crypt_pgp_verify_one (signatures[i], s, tempfile) != 0)
	      goodsig = 0;
	    
	    continue;
	  }

	  if ((WithCrypto & APPLICATION_SMIME)
              && signatures[i]->type == TYPEAPPLICATION 
	      && !mutt_strcasecmp(signatures[i]->subtype, "x-pkcs7-signature"))
	  {
	    if (crypt_smime_verify_one (signatures[i], s, tempfile) != 0)
	      goodsig = 0;
	    
	    continue;
	  }

	  state_printf (s, _("[-- Warning: "
                             "We can't verify %s/%s signatures. --]\n\n"),
			  TYPE(signatures[i]), signatures[i]->subtype);
	}
      }
      
      mutt_unlink (tempfile);

      b->goodsig = goodsig;
      b->badsig = goodsig;		/* XXX - WHAT!?!?!? */
      
      /* Now display the signed body */
      state_attach_puts (_("[-- The following data is signed --]\n\n"), s);


      FREE (&signatures);
    }
    else
      state_attach_puts (_("[-- Warning: Can't find any signatures. --]\n\n"), s);
  }
  
  mutt_body_handler (a, s);
  
  if (s->flags & M_DISPLAY && sigcnt)
    state_attach_puts (_("\n[-- End of signed data --]\n"), s);
}


