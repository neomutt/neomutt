/*
 * Copyright (C) 1996,1997 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@guug.de>
 * Copyright (C) 2001  Thomas Roessler <roessler@guug.de>
 *                     Oliver Ehli <elmy@acm.org>
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
#include "crypt.h"
#include "mime.h"
#include "copy.h"

#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#ifdef HAVE_PGP
#include "pgp.h"
#endif

#ifdef HAVE_SMIME
#include "smime.h"
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#if defined(HAVE_PGP) || defined(HAVE_SMIME)


/* print the current time to avoid spoofing of the signature output */
void crypt_current_time(STATE *s, char *app_name)
{
  time_t t;
  char p[STRING], tmp[STRING];

  t = time(NULL);
  setlocale (LC_TIME, "");
  snprintf (tmp, sizeof (tmp), _("[-- %s output follows(current time: %%c) --]\n"), NONULL(app_name));
  strftime (p, sizeof (p), tmp, localtime (&t));
  setlocale (LC_TIME, "C");
  state_attach_puts (p, s);
}



void crypt_forget_passphrase (void)
{
    
#ifdef HAVE_PGP
  pgp_void_passphrase ();
#endif

#ifdef HAVE_SMIME
  smime_void_passphrase ();
#endif

  mutt_message _("Passphrase(s) forgotten.");
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


int crypt_valid_passphrase(int flags)
{
  time_t now = time (NULL);

# if defined(HAVE_SETRLIMIT) &&(!defined(DEBUG))
  disable_coredumps ();
# endif


#ifdef HAVE_PGP
  if (flags & APPLICATION_PGP)
  {
    extern char PgpPass[STRING];
    extern time_t PgpExptime;

    if (now < PgpExptime) return 1; /* just use the cached copy. */
    pgp_void_passphrase ();
      
    if (mutt_get_password (_("Enter PGP passphrase:"), PgpPass, sizeof (PgpPass)) == 0)
    {
      PgpExptime = time (NULL) + PgpTimeout;
      return (1);
    }
    else
      PgpExptime = 0;
    }
#endif
#ifdef HAVE_SMIME
  if (flags & APPLICATION_SMIME)
  {
    extern char SmimePass[STRING];
    extern time_t SmimeExptime;

    if (now < SmimeExptime) return (1);
    smime_void_passphrase ();
      
    if (mutt_get_password (_("Enter SMIME passphrase:"), SmimePass,
			   sizeof (SmimePass)) == 0)
    {
      SmimeExptime = time (NULL) + SmimeTimeout;
      return (1);
    }
    else
      SmimeExptime = 0;
  }
#endif
  return (0);
}



int mutt_protect (HEADER *msg, char *keylist)
{
  BODY *pbody = NULL, *tmp_pbody = NULL;
#ifdef HAVE_SMIME
  BODY *tmp_smime_pbody = NULL;
#endif
#ifdef HAVE_PGP
  BODY *tmp_pgp_pbody = NULL;
  int traditional = 0;
  int flags = msg->security, i;
#endif
  if ((msg->security & SIGN) && !crypt_valid_passphrase (msg->security))
    return (-1);

#ifdef HAVE_PGP
  if (msg->security & APPLICATION_PGP)
  {
    if ((msg->content->type == TYPETEXT) &&
	!mutt_strcasecmp (msg->content->subtype, "plain") &&
	((flags & ENCRYPT) || (msg->content->content && msg->content->content->hibin == 0)))
    {
      if ((i = query_quadoption (OPT_PGPTRADITIONAL, _("Create an application/pgp message?"))) == -1)
	return -1;
      else if (i == M_YES)
	traditional = 1;
    }
    if (traditional)
    {
      mutt_message _("Invoking PGP...");
      if (!(pbody = pgp_traditional_encryptsign (msg->content, flags, keylist)))
	return -1;
    
      msg->content = pbody;
      return 0;
    }
  }
#endif

  if (!isendwin ()) mutt_endwin (NULL);

#ifdef HAVE_SMIME
  tmp_smime_pbody = msg->content;
#endif


  if (msg->security & SIGN)
  {
#ifdef HAVE_SMIME
    if (msg->security & APPLICATION_SMIME)
    {
      if (!(tmp_pbody = smime_sign_message (msg->content)))
	return -1;
      pbody = tmp_smime_pbody = tmp_pbody;
    }
#endif
#ifdef HAVE_PGP
    if ((msg->security & APPLICATION_PGP) &&
        (!(flags & ENCRYPT) || option (OPTPGPRETAINABLESIG)))
    {
      if (!(tmp_pbody = pgp_sign_message (msg->content)))
        return -1;

      flags &= ~SIGN;
      pbody = tmp_pgp_pbody = tmp_pbody;
    }
#endif

#if defined(HAVE_SMIME) && defined(HAVE_PGP)
    if ((msg->security & APPLICATION_SMIME) &&
	(msg->security & APPLICATION_PGP))
    {
	/* here comes the draft ;-) */
    }
#endif
  }


  if (msg->security & ENCRYPT)
  {
#ifdef HAVE_SMIME
    if (msg->security & APPLICATION_SMIME)
    {
      if (!(tmp_pbody = smime_build_smime_entity (tmp_smime_pbody, keylist)))
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
#endif

#ifdef HAVE_PGP
    if (msg->security & APPLICATION_PGP)
    {
      if (!(pbody = pgp_encrypt_message (msg->content, keylist, flags & SIGN)))
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
#endif
  }

  if(pbody)
      msg->content = pbody;

  return 0;
}


   
     
int mutt_is_multipart_signed (BODY *b)
{
  char *p;

  if (!b || !(b->type == TYPEMULTIPART) ||
      !b->subtype || mutt_strcasecmp(b->subtype, "signed"))
    return 0;

  if (!(p = mutt_get_parameter("protocol", b->parameter)))
    return 0;

  if (!(mutt_strcasecmp (p, "multipart/mixed")))
    return SIGN;

#ifdef HAVE_PGP
  if (!(mutt_strcasecmp (p, "application/pgp-signature")))
    return PGPSIGN;
#endif
    
#ifdef HAVE_SMIME
  if (!(mutt_strcasecmp(p, "application/x-pkcs7-signature")))
    return SMIMESIGN;
#endif

  return 0;
}
   
     
int mutt_is_multipart_encrypted (BODY *b)
{
  int ret=0;
#ifdef HAVE_PGP
  ret = pgp_is_multipart_encrypted (b);
#endif

  return ret;
}




int crypt_query (BODY *m)
{
  int t = 0;


  if (m->type == TYPEAPPLICATION)
  {
#ifdef HAVE_PGP
    t |= mutt_is_application_pgp(m);
#endif
#ifdef HAVE_SMIME
    t |= mutt_is_application_smime(m);
    if (t && m->goodsig) t |= GOODSIGN;
    if (t && m->badsig) t |= BADSIGN;
#endif
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
  while (a)
  {
    if (a->type == TYPEMULTIPART)
    {
      if (a->encoding != ENC7BIT)
      {
        a->encoding = ENC7BIT;
	convert_to_7bit(a->parts);
      }
#ifdef HAVE_PGP
      else if (option (OPTPGPSTRICTENC))
	convert_to_7bit (a->parts);
#endif
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

  mutt_mktemp (tempfname);
  if (!(fpout = safe_fopen (tempfname, "w")))
  {
    mutt_perror (tempfname);
    return;
  }

#ifdef HAVE_PGP
  set_option (OPTDONTHANDLEPGPKEYS);
#endif  

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
#ifdef HAVE_PGP
	if (Context->hdrs[Context->v2r[i]]->security & APPLICATION_PGP)
	{
	  mutt_copy_message (fpout, Context, Context->hdrs[Context->v2r[i]], 
			     M_CM_DECODE|M_CM_CHARCONV, 0);
	  fflush(fpout);
	  
	  mutt_endwin (_("Trying to extract PGP keys...\n"));
	  pgp_invoke_import (tempfname);
	}
#endif
#ifdef HAVE_SMIME
	if (Context->hdrs[Context->v2r[i]]->security & APPLICATION_SMIME)
	{
	  if (Context->hdrs[Context->v2r[i]]->security & ENCRYPT)
	    mutt_copy_message (fpout, Context, Context->hdrs[Context->v2r[i]],
			       M_CM_NOHEADER|M_CM_DECODE_CRYPT|M_CM_DECODE_SMIME, 0);
	  else
	    mutt_copy_message (fpout, Context,
			       Context->hdrs[Context->v2r[i]], 0, 0);
	  fflush(fpout);

          if (Context->hdrs[Context->v2r[i]]->env->from)
	    tmp = mutt_expand_aliases (h->env->from);
	  else if (Context->hdrs[Context->v2r[i]]->env->sender)
	    tmp = mutt_expand_aliases (Context->hdrs[Context->v2r[i]]->env->sender);
          mbox = tmp ? tmp->mailbox : NULL;
	  if (mbox)
	  {
	    mutt_endwin (_("Trying to extract S/MIME certificates...\n"));
	    smime_invoke_import (tempfname, mbox);
	    tmp = NULL;
	  }
	}
#endif
	rewind (fpout);
      }
    }
  }
  else
  {
    mutt_parse_mime_message (Context, h);
    if (!(h->security & ENCRYPT && !crypt_valid_passphrase (h->security)))
    {
#ifdef HAVE_PGP
      if (h->security & APPLICATION_PGP)
      {
	mutt_copy_message (fpout, Context, h, M_CM_DECODE|M_CM_CHARCONV, 0);
	fflush(fpout);
	mutt_message (_("Trying to extract PGP keys...\n"));
	pgp_invoke_import (tempfname);
      }
#endif  
#ifdef HAVE_SMIME
      if (h->security & APPLICATION_SMIME)
      {
	if (h->security & ENCRYPT)
	  mutt_copy_message (fpout, Context, h, M_CM_NOHEADER|M_CM_DECODE_CRYPT|M_CM_DECODE_SMIME, 0);
	else
	  mutt_copy_message (fpout, Context, h, 0, 0);

	fflush(fpout);
	if (h->env->from) tmp = mutt_expand_aliases (h->env->from);
	else if (h->env->sender)  tmp = mutt_expand_aliases (h->env->sender); 
	mbox = tmp ? tmp->mailbox : NULL;
	if (mbox) /* else ? */
	{
	  mutt_message (_("Trying to extract S/MIME certificates...\n"));
	  smime_invoke_import (tempfname, mbox);
	}
      }
#endif  
    }
  }
      
  fclose (fpout);
  mutt_any_key_to_continue (NULL);

  mutt_unlink (tempfname);

#ifdef HAVE_PGP
  unset_option (OPTDONTHANDLEPGPKEYS);
#endif  
}



int crypt_get_keys (HEADER *msg, char **keylist)
{
  /* Do a quick check to make sure that we can find all of the encryption
   * keys if the user has requested this service.
   */

#ifdef HAVE_SMIME
    extern char *smime_findKeys (ADDRESS *to, ADDRESS *cc, ADDRESS *bcc);
#endif
#ifdef HAVE_PGP
    extern char *pgp_findKeys (ADDRESS *to, ADDRESS *cc, ADDRESS *bcc);

    set_option (OPTPGPCHECKTRUST);

#endif

    *keylist = NULL;


    if (msg->security & ENCRYPT)
    {
#ifdef HAVE_PGP
       if (msg->security & APPLICATION_PGP)
       {
         if ((*keylist = pgp_findKeys (msg->env->to, msg->env->cc,
				       msg->env->bcc)) == NULL)
	     return (-1);
	 unset_option (OPTPGPCHECKTRUST);
       }
#endif
#ifdef HAVE_SMIME
       if (msg->security & APPLICATION_SMIME)
       {
	 if ((*keylist = smime_findKeys (msg->env->to, msg->env->cc,
					 msg->env->bcc)) == NULL)
	     return (-1);
       }
#endif
    }
    
    return (0);
}



static void crypt_fetch_signatures (BODY ***signatures, BODY *a, int *n)
{
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
    state_attach_puts (_("[-- Error: Inconsistent multipart/signed structure! --]\n\n"), s);
    mutt_body_handler (a, s);
    return;
  }

  
#ifdef HAVE_PGP
  if (protocol_major == TYPEAPPLICATION &&
      !mutt_strcasecmp (protocol_minor, "pgp-signature"));
#endif
#if defined(HAVE_PGP) && defined(HAVE_SMIME)
  else
#endif
#ifdef HAVE_SMIME
        if (protocol_major == TYPEAPPLICATION &&
	    !mutt_strcasecmp (protocol_minor, "x-pkcs7-signature"));
  
#endif
#if defined(HAVE_PGP) || defined(HAVE_SMIME)
  else
#endif
        if (protocol_major == TYPEMULTIPART &&
	    !mutt_strcasecmp (protocol_minor, "mixed"));

  else
  {
    state_printf (s, _("[-- Error: Unknown multipart/signed protocol %s! --]\n\n"), protocol);
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
#ifdef HAVE_PGP
	  if (signatures[i]->type == TYPEAPPLICATION 
	      && !mutt_strcasecmp (signatures[i]->subtype, "pgp-signature"))
	  {
	    if (pgp_verify_one (signatures[i], s, tempfile) != 0)
	      goodsig = 0;
	    
	    continue;
	  }
#endif
#ifdef HAVE_SMIME
	  if (signatures[i]->type == TYPEAPPLICATION 
	      && !mutt_strcasecmp(signatures[i]->subtype, "x-pkcs7-signature"))
	  {
	    if (smime_verify_one (signatures[i], s, tempfile) != 0)
	      goodsig = 0;
	    
	    continue;
	  }
#endif
	  state_printf (s, _("[-- Warning: We can't verify %s/%s signatures. --]\n\n"),
			  TYPE(signatures[i]), signatures[i]->subtype);
	}
      }
      
      mutt_unlink (tempfile);

      b->goodsig = goodsig;
      b->badsig = goodsig;
      
      /* Now display the signed body */
      state_attach_puts (_("[-- The following data is signed --]\n\n"), s);


      safe_free((void **) &signatures);
    }
    else
      state_attach_puts (_("[-- Warning: Can't find any signatures. --]\n\n"), s);
  }
  
  mutt_body_handler (a, s);
  
  if (s->flags & M_DISPLAY && sigcnt)
    state_attach_puts (_("\n[-- End of signed data --]\n"), s);
}



#endif
