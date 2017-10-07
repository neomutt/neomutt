/**
 * @file
 * Signing/encryption multiplexor
 *
 * @authors
 * Copyright (C) 1996-1997 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2000,2002-2004,2006 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2001 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2001 Oliver Ehli <elmy@acm.org>
 * Copyright (C) 2003 Werner Koch <wk@gnupg.org>
 * Copyright (C) 2004 g10code GmbH
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "lib/lib.h"
#include "mutt.h"
#include "address.h"
#include "alias.h"
#include "body.h"
#include "content.h"
#include "context.h"
#include "copy.h"
#include "cryptglue.h"
#include "envelope.h"
#include "globals.h"
#include "header.h"
#include "mime.h"
#include "mutt_curses.h"
#include "ncrypt.h"
#include "options.h"
#include "parameter.h"
#include "protos.h"
#include "rfc822.h"
#include "state.h"

/**
 * crypt_current_time - Print the current time
 *
 * print the current time to avoid spoofing of the signature output
 */
void crypt_current_time(struct State *s, char *app_name)
{
  time_t t;
  char p[STRING], tmp[STRING];

  if (!WithCrypto)
    return;

  if (option(OPT_CRYPT_TIMESTAMP))
  {
    t = time(NULL);
    strftime(p, sizeof(p), _(" (current time: %c)"), localtime(&t));
  }
  else
    *p = '\0';

  snprintf(tmp, sizeof(tmp), _("[-- %s output follows%s --]\n"), NONULL(app_name), p);
  state_attach_puts(tmp, s);
}

/**
 * crypt_forget_passphrase - Forget a passphrase and display a message
 */
void crypt_forget_passphrase(void)
{
  if ((WithCrypto & APPLICATION_PGP))
    crypt_pgp_void_passphrase();

  if ((WithCrypto & APPLICATION_SMIME))
    crypt_smime_void_passphrase();

  if (WithCrypto)
    mutt_message(_("Passphrase(s) forgotten."));
}

#if (!defined(DEBUG))

#include <sys/resource.h>
static void disable_coredumps(void)
{
  struct rlimit rl = { 0, 0 };
  static bool done = false;

  if (!done)
  {
    setrlimit(RLIMIT_CORE, &rl);
    done = true;
  }
}

#endif

/**
 * crypt_valid_passphrase - Check that we have a usable passphrase, ask if not
 */
int crypt_valid_passphrase(int flags)
{
  int ret = 0;

#if !defined(DEBUG)
  disable_coredumps();
#endif

  if ((WithCrypto & APPLICATION_PGP) && (flags & APPLICATION_PGP))
    ret = crypt_pgp_valid_passphrase();

  if ((WithCrypto & APPLICATION_SMIME) && (flags & APPLICATION_SMIME))
    ret = crypt_smime_valid_passphrase();

  return ret;
}

int mutt_protect(struct Header *msg, char *keylist)
{
  struct Body *pbody = NULL, *tmp_pbody = NULL;
  struct Body *tmp_smime_pbody = NULL;
  struct Body *tmp_pgp_pbody = NULL;
  int flags = (WithCrypto & APPLICATION_PGP) ? msg->security : 0;

  if (!WithCrypto)
    return -1;

  if (!(msg->security & (ENCRYPT | SIGN)))
    return 0;

  if ((msg->security & SIGN) && !crypt_valid_passphrase(msg->security))
    return -1;

  if ((WithCrypto & APPLICATION_PGP) && ((msg->security & PGPINLINE) == PGPINLINE))
  {
    if ((msg->content->type != TYPETEXT) ||
        (mutt_strcasecmp(msg->content->subtype, "plain") != 0))
    {
      if (query_quadoption(OPT_PGP_MIME_AUTO,
                           _("Inline PGP can't be used with attachments.  "
                             "Revert to PGP/MIME?")) != MUTT_YES)
      {
        mutt_error(
            _("Mail not sent: inline PGP can't be used with attachments."));
        return -1;
      }
    }
    else if (!mutt_strcasecmp("flowed",
                              mutt_get_parameter("format", msg->content->parameter)))
    {
      if ((query_quadoption(OPT_PGP_MIME_AUTO,
                            _("Inline PGP can't be used with format=flowed.  "
                              "Revert to PGP/MIME?"))) != MUTT_YES)
      {
        mutt_error(
            _("Mail not sent: inline PGP can't be used with format=flowed."));
        return -1;
      }
    }
    else
    {
      /* they really want to send it inline... go for it */
      if (!isendwin())
        mutt_endwin(_("Invoking PGP..."));
      pbody = crypt_pgp_traditional_encryptsign(msg->content, flags, keylist);
      if (pbody)
      {
        msg->content = pbody;
        return 0;
      }

      /* otherwise inline won't work...ask for revert */
      if (query_quadoption(
              OPT_PGP_MIME_AUTO,
              _("Message can't be sent inline.  Revert to using PGP/MIME?")) != MUTT_YES)
      {
        mutt_error(_("Mail not sent."));
        return -1;
      }
    }

    /* go ahead with PGP/MIME */
  }

  if (!isendwin())
    mutt_endwin(NULL);

  if ((WithCrypto & APPLICATION_SMIME))
    tmp_smime_pbody = msg->content;
  if ((WithCrypto & APPLICATION_PGP))
    tmp_pgp_pbody = msg->content;

  if (option(OPT_CRYPT_USE_PKA) && (msg->security & SIGN))
  {
    /* Set sender (necessary for e.g. PKA).  */
    const char *mailbox = NULL;
    struct Address *from = msg->env->from;

    if (!from)
      from = mutt_default_from();

    mailbox = from->mailbox;
    if (!mailbox && EnvelopeFromAddress)
      mailbox = EnvelopeFromAddress->mailbox;

    if ((WithCrypto & APPLICATION_SMIME) && (msg->security & APPLICATION_SMIME))
      crypt_smime_set_sender(mailbox);
    else if ((WithCrypto & APPLICATION_PGP) && (msg->security & APPLICATION_PGP))
      crypt_pgp_set_sender(mailbox);

    if (!msg->env->from)
      rfc822_free_address(&from);
  }

  if (msg->security & SIGN)
  {
    if ((WithCrypto & APPLICATION_SMIME) && (msg->security & APPLICATION_SMIME))
    {
      tmp_pbody = crypt_smime_sign_message(msg->content);
      if (!tmp_pbody)
        return -1;
      pbody = tmp_smime_pbody = tmp_pbody;
    }

    if ((WithCrypto & APPLICATION_PGP) && (msg->security & APPLICATION_PGP) &&
        (!(flags & ENCRYPT) || option(OPT_PGP_RETAINABLE_SIGS)))
    {
      tmp_pbody = crypt_pgp_sign_message(msg->content);
      if (!tmp_pbody)
        return -1;

      flags &= ~SIGN;
      pbody = tmp_pgp_pbody = tmp_pbody;
    }

    if (WithCrypto && (msg->security & APPLICATION_SMIME) && (msg->security & APPLICATION_PGP))
    {
      /* here comes the draft ;-) */
    }
  }

  if (msg->security & ENCRYPT)
  {
    if ((WithCrypto & APPLICATION_SMIME) && (msg->security & APPLICATION_SMIME))
    {
      tmp_pbody = crypt_smime_build_smime_entity(tmp_smime_pbody, keylist);
      if (!tmp_pbody)
      {
        /* signed ? free it! */
        return -1;
      }
      /* free tmp_body if messages was signed AND encrypted ... */
      if (tmp_smime_pbody != msg->content && tmp_smime_pbody != tmp_pbody)
      {
        /* detach and don't delete msg->content,
           which tmp_smime_pbody->parts after signing. */
        tmp_smime_pbody->parts = tmp_smime_pbody->parts->next;
        msg->content->next = NULL;
        mutt_free_body(&tmp_smime_pbody);
      }
      pbody = tmp_pbody;
    }

    if ((WithCrypto & APPLICATION_PGP) && (msg->security & APPLICATION_PGP))
    {
      pbody = crypt_pgp_encrypt_message(tmp_pgp_pbody, keylist, flags & SIGN);
      if (!pbody)
      {
        /* did we perform a retainable signature? */
        if (flags != msg->security)
        {
          /* remove the outer multipart layer */
          tmp_pgp_pbody = mutt_remove_multipart(tmp_pgp_pbody);
          /* get rid of the signature */
          mutt_free_body(&tmp_pgp_pbody->next);
        }

        return -1;
      }

      /* destroy temporary signature envelope when doing retainable
       * signatures.

       */
      if (flags != msg->security)
      {
        tmp_pgp_pbody = mutt_remove_multipart(tmp_pgp_pbody);
        mutt_free_body(&tmp_pgp_pbody->next);
      }
    }
  }

  if (pbody)
    msg->content = pbody;

  return 0;
}

int mutt_is_multipart_signed(struct Body *b)
{
  char *p = NULL;

  if (!b || !(b->type == TYPEMULTIPART) || !b->subtype ||
      (mutt_strcasecmp(b->subtype, "signed") != 0))
    return 0;

  p = mutt_get_parameter("protocol", b->parameter);
  if (!p)
    return 0;

  if (!(mutt_strcasecmp(p, "multipart/mixed") != 0))
    return SIGN;

  if ((WithCrypto & APPLICATION_PGP) &&
      !(mutt_strcasecmp(p, "application/pgp-signature") != 0))
    return PGPSIGN;

  if ((WithCrypto & APPLICATION_SMIME) &&
      !(mutt_strcasecmp(p, "application/x-pkcs7-signature") != 0))
    return SMIMESIGN;
  if ((WithCrypto & APPLICATION_SMIME) &&
      !(mutt_strcasecmp(p, "application/pkcs7-signature") != 0))
    return SMIMESIGN;

  return 0;
}

int mutt_is_multipart_encrypted(struct Body *b)
{
  if ((WithCrypto & APPLICATION_PGP))
  {
    char *p = NULL;

    if (!b || b->type != TYPEMULTIPART || !b->subtype ||
        (mutt_strcasecmp(b->subtype, "encrypted") != 0) ||
        !(p = mutt_get_parameter("protocol", b->parameter)) ||
        (mutt_strcasecmp(p, "application/pgp-encrypted") != 0))
      return 0;

    return PGPENCRYPT;
  }

  return 0;
}

int mutt_is_valid_multipart_pgp_encrypted(struct Body *b)
{
  if (!mutt_is_multipart_encrypted(b))
    return 0;

  b = b->parts;
  if (!b || b->type != TYPEAPPLICATION || !b->subtype ||
      (mutt_strcasecmp(b->subtype, "pgp-encrypted") != 0))
    return 0;

  b = b->next;
  if (!b || b->type != TYPEAPPLICATION || !b->subtype ||
      (mutt_strcasecmp(b->subtype, "octet-stream") != 0))
    return 0;

  return PGPENCRYPT;
}

/**
 * mutt_is_malformed_multipart_pgp_encrypted - Check for malformed layout
 *
 * This checks for the malformed layout caused by MS Exchange in
 * some cases:
 * ```
 *  <multipart/mixed>
 *     <text/plain>
 *     <application/pgp-encrypted> [BASE64-encoded]
 *     <application/octet-stream> [BASE64-encoded]
 * ```
 * See ticket #3742
 */
int mutt_is_malformed_multipart_pgp_encrypted(struct Body *b)
{
  if (!(WithCrypto & APPLICATION_PGP))
    return 0;

  if (!b || b->type != TYPEMULTIPART || !b->subtype ||
      (mutt_strcasecmp(b->subtype, "mixed") != 0))
    return 0;

  b = b->parts;
  if (!b || b->type != TYPETEXT || !b->subtype ||
      (mutt_strcasecmp(b->subtype, "plain") != 0) || b->length != 0)
    return 0;

  b = b->next;
  if (!b || b->type != TYPEAPPLICATION || !b->subtype ||
      (mutt_strcasecmp(b->subtype, "pgp-encrypted") != 0))
    return 0;

  b = b->next;
  if (!b || b->type != TYPEAPPLICATION || !b->subtype ||
      (mutt_strcasecmp(b->subtype, "octet-stream") != 0))
    return 0;

  b = b->next;
  if (b)
    return 0;

  return PGPENCRYPT;
}

int mutt_is_application_pgp(struct Body *m)
{
  int t = 0;
  char *p = NULL;

  if (m->type == TYPEAPPLICATION)
  {
    if ((mutt_strcasecmp(m->subtype, "pgp") == 0) ||
        (mutt_strcasecmp(m->subtype, "x-pgp-message") == 0))
    {
      if ((p = mutt_get_parameter("x-action", m->parameter)) &&
          ((mutt_strcasecmp(p, "sign") == 0) ||
           (mutt_strcasecmp(p, "signclear") == 0)))
        t |= PGPSIGN;

      if ((p = mutt_get_parameter("format", m->parameter)) &&
          (mutt_strcasecmp(p, "keys-only") == 0))
        t |= PGPKEY;

      if (!t)
        t |= PGPENCRYPT; /* not necessarily correct, but... */
    }

    if (mutt_strcasecmp(m->subtype, "pgp-signed") == 0)
      t |= PGPSIGN;

    if (mutt_strcasecmp(m->subtype, "pgp-keys") == 0)
      t |= PGPKEY;
  }
  else if (m->type == TYPETEXT && (mutt_strcasecmp("plain", m->subtype) == 0))
  {
    if (((p = mutt_get_parameter("x-mutt-action", m->parameter)) ||
         (p = mutt_get_parameter("x-action", m->parameter)) ||
         (p = mutt_get_parameter("action", m->parameter))) &&
        (mutt_strncasecmp("pgp-sign", p, 8) == 0))
      t |= PGPSIGN;
    else if (p && (mutt_strncasecmp("pgp-encrypt", p, 11) == 0))
      t |= PGPENCRYPT;
    else if (p && (mutt_strncasecmp("pgp-keys", p, 7) == 0))
      t |= PGPKEY;
  }
  if (t)
    t |= PGPINLINE;

  return t;
}

int mutt_is_application_smime(struct Body *m)
{
  char *t = NULL;
  int len;
  bool complain = false;

  if (!m)
    return 0;

  if ((m->type & TYPEAPPLICATION) && m->subtype)
  {
    /* S/MIME MIME types don't need x- anymore, see RFC2311 */
    if ((mutt_strcasecmp(m->subtype, "x-pkcs7-mime") == 0) ||
        (mutt_strcasecmp(m->subtype, "pkcs7-mime") == 0))
    {
      if ((t = mutt_get_parameter("smime-type", m->parameter)))
      {
        if (mutt_strcasecmp(t, "enveloped-data") == 0)
          return SMIMEENCRYPT;
        else if (mutt_strcasecmp(t, "signed-data") == 0)
          return (SMIMESIGN | SMIMEOPAQUE);
        else
          return 0;
      }
      /* Netscape 4.7 uses
       * Content-Description: S/MIME Encrypted Message
       * instead of Content-Type parameter
       */
      if (mutt_strcasecmp(m->description, "S/MIME Encrypted Message") == 0)
        return SMIMEENCRYPT;
      complain = true;
    }
    else if (mutt_strcasecmp(m->subtype, "octet-stream") != 0)
      return 0;

    t = mutt_get_parameter("name", m->parameter);

    if (!t)
      t = m->d_filename;
    if (!t)
      t = m->filename;
    if (!t)
    {
      if (complain)
        mutt_message(
            _("S/MIME messages with no hints on content are unsupported."));
      return 0;
    }

    /* no .p7c, .p10 support yet. */

    len = mutt_strlen(t) - 4;
    if (len > 0 && *(t + len) == '.')
    {
      len++;
      if (mutt_strcasecmp((t + len), "p7m") == 0)
        /* Not sure if this is the correct thing to do, but
         it's required for compatibility with Outlook */
        return (SMIMESIGN | SMIMEOPAQUE);
      else if (mutt_strcasecmp((t + len), "p7s") == 0)
        return (SMIMESIGN | SMIMEOPAQUE);
    }
  }

  return 0;
}

/**
 * crypt_query - Check out the type of encryption used
 *
 * Set the cached status values if there are any.
 */
int crypt_query(struct Body *m)
{
  int t = 0;

  if (!WithCrypto)
    return 0;

  if (!m)
    return 0;

  if (m->type == TYPEAPPLICATION)
  {
    if ((WithCrypto & APPLICATION_PGP))
      t |= mutt_is_application_pgp(m);

    if ((WithCrypto & APPLICATION_SMIME))
    {
      t |= mutt_is_application_smime(m);
      if (t && m->goodsig)
        t |= GOODSIGN;
      if (t && m->badsig)
        t |= BADSIGN;
    }
  }
  else if ((WithCrypto & APPLICATION_PGP) && m->type == TYPETEXT)
  {
    t |= mutt_is_application_pgp(m);
    if (t && m->goodsig)
      t |= GOODSIGN;
  }

  if (m->type == TYPEMULTIPART)
  {
    t |= mutt_is_multipart_encrypted(m);
    t |= mutt_is_multipart_signed(m);
    t |= mutt_is_malformed_multipart_pgp_encrypted(m);

    if (t && m->goodsig)
      t |= GOODSIGN;
  }

  if (m->type == TYPEMULTIPART || m->type == TYPEMESSAGE)
  {
    int u, v, w;

    u = m->parts ? 0xffffffff : 0; /* Bits set in all parts */
    w = 0;                         /* Bits set in any part  */

    for (struct Body *b = m->parts; b; b = b->next)
    {
      v = crypt_query(b);
      u &= v;
      w |= v;
    }
    t |= u | (w & ~GOODSIGN);

    if ((w & GOODSIGN) && !(u & GOODSIGN))
      t |= PARTSIGN;
  }

  return t;
}

/**
 * crypt_write_signed - Write the message body/part
 *
 * Body/part A described by state S to the given TEMPFILE.
 */
int crypt_write_signed(struct Body *a, struct State *s, const char *tempfile)
{
  FILE *fp = NULL;
  int c;
  bool hadcr;
  size_t bytes;

  if (!WithCrypto)
    return -1;

  fp = safe_fopen(tempfile, "w");
  if (!fp)
  {
    mutt_perror(tempfile);
    return -1;
  }

  fseeko(s->fpin, a->hdr_offset, SEEK_SET);
  bytes = a->length + a->offset - a->hdr_offset;
  hadcr = false;
  while (bytes > 0)
  {
    c = fgetc(s->fpin);
    if (c == EOF)
      break;

    bytes--;

    if (c == '\r')
      hadcr = true;
    else
    {
      if (c == '\n' && !hadcr)
        fputc('\r', fp);

      hadcr = false;
    }

    fputc(c, fp);
  }
  safe_fclose(&fp);

  return 0;
}

void convert_to_7bit(struct Body *a)
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
      else if ((WithCrypto & APPLICATION_PGP) && option(OPT_PGP_STRICT_ENC))
        convert_to_7bit(a->parts);
    }
    else if (a->type == TYPEMESSAGE &&
             (mutt_strcasecmp(a->subtype, "delivery-status") != 0))
    {
      if (a->encoding != ENC7BIT)
        mutt_message_to_7bit(a, NULL);
    }
    else if (a->encoding == ENC8BIT)
      a->encoding = ENCQUOTEDPRINTABLE;
    else if (a->encoding == ENCBINARY)
      a->encoding = ENCBASE64;
    else if (a->content && a->encoding != ENCBASE64 &&
             (a->content->from || (a->content->space && option(OPT_PGP_STRICT_ENC))))
      a->encoding = ENCQUOTEDPRINTABLE;
    a = a->next;
  }
}

void crypt_extract_keys_from_messages(struct Header *h)
{
  char tempfname[_POSIX_PATH_MAX], *mbox = NULL;
  struct Address *tmp = NULL;
  FILE *fpout = NULL;

  if (!WithCrypto)
    return;

  mutt_mktemp(tempfname, sizeof(tempfname));
  fpout = safe_fopen(tempfname, "w");
  if (!fpout)
  {
    mutt_perror(tempfname);
    return;
  }

  if ((WithCrypto & APPLICATION_PGP))
    set_option(OPT_DONT_HANDLE_PGP_KEYS);

  if (!h)
  {
    for (int i = 0; i < Context->msgcount; i++)
    {
      if (message_is_tagged(Context, i))
      {
      struct Header *h = Context->hdrs[i];

      mutt_parse_mime_message(Context, h);
      if (h->security & ENCRYPT && !crypt_valid_passphrase(h->security))
      {
        safe_fclose(&fpout);
        break;
      }

      if ((WithCrypto & APPLICATION_PGP) && (h->security & APPLICATION_PGP))
      {
        mutt_copy_message(fpout, Context, h, MUTT_CM_DECODE | MUTT_CM_CHARCONV, 0);
        fflush(fpout);

        mutt_endwin(_("Trying to extract PGP keys...\n"));
        crypt_pgp_invoke_import(tempfname);
      }

      if ((WithCrypto & APPLICATION_SMIME) && (h->security & APPLICATION_SMIME))
      {
        if (h->security & ENCRYPT)
          mutt_copy_message(fpout, Context, h,
                            MUTT_CM_NOHEADER | MUTT_CM_DECODE_CRYPT | MUTT_CM_DECODE_SMIME,
                            0);
        else
          mutt_copy_message(fpout, Context, h, 0, 0);
        fflush(fpout);

        if (h->env->from)
          tmp = mutt_expand_aliases(h->env->from);
        else if (h->env->sender)
          tmp = mutt_expand_aliases(h->env->sender);
        mbox = tmp ? tmp->mailbox : NULL;
        if (mbox)
        {
          mutt_endwin(_("Trying to extract S/MIME certificates...\n"));
          crypt_smime_invoke_import(tempfname, mbox);
          tmp = NULL;
        }
      }

      rewind(fpout);
      }
    }
  }
  else
  {
    mutt_parse_mime_message(Context, h);
    if (!(h->security & ENCRYPT && !crypt_valid_passphrase(h->security)))
    {
      if ((WithCrypto & APPLICATION_PGP) && (h->security & APPLICATION_PGP))
      {
        mutt_copy_message(fpout, Context, h, MUTT_CM_DECODE | MUTT_CM_CHARCONV, 0);
        fflush(fpout);
        mutt_endwin(_("Trying to extract PGP keys...\n"));
        crypt_pgp_invoke_import(tempfname);
      }

      if ((WithCrypto & APPLICATION_SMIME) && (h->security & APPLICATION_SMIME))
      {
        if (h->security & ENCRYPT)
          mutt_copy_message(fpout, Context, h,
                            MUTT_CM_NOHEADER | MUTT_CM_DECODE_CRYPT | MUTT_CM_DECODE_SMIME,
                            0);
        else
          mutt_copy_message(fpout, Context, h, 0, 0);

        fflush(fpout);
        if (h->env->from)
          tmp = mutt_expand_aliases(h->env->from);
        else if (h->env->sender)
          tmp = mutt_expand_aliases(h->env->sender);
        mbox = tmp ? tmp->mailbox : NULL;
        if (mbox) /* else ? */
        {
          mutt_message(_("Trying to extract S/MIME certificates...\n"));
          crypt_smime_invoke_import(tempfname, mbox);
        }
      }
    }
  }

  safe_fclose(&fpout);
  if (isendwin())
    mutt_any_key_to_continue(NULL);

  mutt_unlink(tempfname);

  if ((WithCrypto & APPLICATION_PGP))
    unset_option(OPT_DONT_HANDLE_PGP_KEYS);
}

/**
 * crypt_get_keys - Check we have all the keys we need
 *
 * Do a quick check to make sure that we can find all of the
 * encryption keys if the user has requested this service.
 * Return the list of keys in KEYLIST.
 * If oppenc_mode is true, only keys that can be determined without
 * prompting will be used.
 */
int crypt_get_keys(struct Header *msg, char **keylist, int oppenc_mode)
{
  struct Address *adrlist = NULL, *last = NULL;
  const char *fqdn = mutt_fqdn(1);
  char *self_encrypt = NULL;
  size_t keylist_size;

  /* Do a quick check to make sure that we can find all of the encryption
   * keys if the user has requested this service.
   */

  if (!WithCrypto)
    return 0;

  if ((WithCrypto & APPLICATION_PGP))
    set_option(OPT_PGP_CHECK_TRUST);

  last = rfc822_append(&adrlist, msg->env->to, 0);
  last = rfc822_append(last ? &last : &adrlist, msg->env->cc, 0);
  rfc822_append(last ? &last : &adrlist, msg->env->bcc, 0);

  if (fqdn)
    rfc822_qualify(adrlist, fqdn);
  adrlist = mutt_remove_duplicates(adrlist);

  *keylist = NULL;

  if (oppenc_mode || (msg->security & ENCRYPT))
  {
    if ((WithCrypto & APPLICATION_PGP) && (msg->security & APPLICATION_PGP))
    {
      *keylist = crypt_pgp_findkeys(adrlist, oppenc_mode);
      if (!*keylist)
      {
        rfc822_free_address(&adrlist);
        return -1;
      }
      unset_option(OPT_PGP_CHECK_TRUST);
      if (option(OPT_PGP_SELF_ENCRYPT) || (quadoption(OPT_PGP_ENCRYPT_SELF) == MUTT_YES))
        self_encrypt = PgpSelfEncryptAs;
    }
    if ((WithCrypto & APPLICATION_SMIME) && (msg->security & APPLICATION_SMIME))
    {
      *keylist = crypt_smime_findkeys(adrlist, oppenc_mode);
      if (!*keylist)
      {
        rfc822_free_address(&adrlist);
        return -1;
      }
      if (option(OPT_SMIME_SELF_ENCRYPT) || (quadoption(OPT_SMIME_ENCRYPT_SELF) == MUTT_YES))
        self_encrypt = SmimeSelfEncryptAs;
    }
  }

  if (!oppenc_mode && self_encrypt && *self_encrypt)
  {
    keylist_size = mutt_strlen(*keylist);
    safe_realloc(keylist, keylist_size + mutt_strlen(self_encrypt) + 2);
    sprintf(*keylist + keylist_size, " %s", self_encrypt);
  }

  rfc822_free_address(&adrlist);

  return 0;
}

/**
 * crypt_opportunistic_encrypt - Can all recipients be determined
 *
 * Check if all recipients keys can be automatically determined.
 * Enable encryption if they can, otherwise disable encryption.
 */
void crypt_opportunistic_encrypt(struct Header *msg)
{
  char *pgpkeylist = NULL;

  if (!WithCrypto)
    return;

  if (!(option(OPT_CRYPT_OPPORTUNISTIC_ENCRYPT) && (msg->security & OPPENCRYPT)))
    return;

  crypt_get_keys(msg, &pgpkeylist, 1);
  if (pgpkeylist)
  {
    msg->security |= ENCRYPT;
    FREE(&pgpkeylist);
  }
  else
  {
    msg->security &= ~ENCRYPT;
  }
}

static void crypt_fetch_signatures(struct Body ***signatures, struct Body *a, int *n)
{
  if (!WithCrypto)
    return;

  for (; a; a = a->next)
  {
    if (a->type == TYPEMULTIPART)
      crypt_fetch_signatures(signatures, a->parts, n);
    else
    {
      if ((*n % 5) == 0)
        safe_realloc(signatures, (*n + 6) * sizeof(struct Body **));

      (*signatures)[(*n)++] = a;
    }
  }
}

/**
 * mutt_signed_handler - Verify a "multipart/signed" body
 */
int mutt_signed_handler(struct Body *a, struct State *s)
{
  char tempfile[_POSIX_PATH_MAX];
  int signed_type;
  bool inconsistent = false;

  struct Body *b = a;
  struct Body **signatures = NULL;
  int sigcnt = 0;
  bool goodsig = true;
  int rc = 0;

  if (!WithCrypto)
    return -1;

  a = a->parts;
  signed_type = mutt_is_multipart_signed(b);
  if (!signed_type)
  {
    /* A null protocol value is already checked for in mutt_body_handler() */
    state_printf(s, _("[-- Error: "
                      "Unknown multipart/signed protocol %s! --]\n\n"),
                 mutt_get_parameter("protocol", b->parameter));
    return mutt_body_handler(a, s);
  }

  if (!(a && a->next))
    inconsistent = true;
  else
  {
    switch (signed_type)
    {
      case SIGN:
        if (a->next->type != TYPEMULTIPART ||
            (mutt_strcasecmp(a->next->subtype, "mixed") != 0))
          inconsistent = true;
        break;
      case PGPSIGN:
        if (a->next->type != TYPEAPPLICATION ||
            (mutt_strcasecmp(a->next->subtype, "pgp-signature") != 0))
          inconsistent = true;
        break;
      case SMIMESIGN:
        if (a->next->type != TYPEAPPLICATION ||
            ((mutt_strcasecmp(a->next->subtype, "x-pkcs7-signature") != 0) &&
             (mutt_strcasecmp(a->next->subtype, "pkcs7-signature") != 0)))
          inconsistent = true;
        break;
      default:
        inconsistent = true;
    }
  }
  if (inconsistent)
  {
    state_attach_puts(_("[-- Error: "
                        "Inconsistent multipart/signed structure! --]\n\n"),
                      s);
    return mutt_body_handler(a, s);
  }

  if (s->flags & MUTT_DISPLAY)
  {
    crypt_fetch_signatures(&signatures, a->next, &sigcnt);

    if (sigcnt)
    {
      mutt_mktemp(tempfile, sizeof(tempfile));
      if (crypt_write_signed(a, s, tempfile) == 0)
      {
        for (int i = 0; i < sigcnt; i++)
        {
          if ((WithCrypto & APPLICATION_PGP) && signatures[i]->type == TYPEAPPLICATION &&
              (mutt_strcasecmp(signatures[i]->subtype, "pgp-signature") == 0))
          {
            if (crypt_pgp_verify_one(signatures[i], s, tempfile) != 0)
              goodsig = false;

            continue;
          }

          if ((WithCrypto & APPLICATION_SMIME) && signatures[i]->type == TYPEAPPLICATION &&
              ((mutt_strcasecmp(signatures[i]->subtype, "x-pkcs7-signature") == 0) ||
               (mutt_strcasecmp(signatures[i]->subtype, "pkcs7-signature") == 0)))
          {
            if (crypt_smime_verify_one(signatures[i], s, tempfile) != 0)
              goodsig = false;

            continue;
          }

          state_printf(s, _("[-- Warning: "
                            "We can't verify %s/%s signatures. --]\n\n"),
                       TYPE(signatures[i]), signatures[i]->subtype);
        }
      }

      mutt_unlink(tempfile);

      b->goodsig = goodsig;
      b->badsig = !goodsig;

      /* Now display the signed body */
      state_attach_puts(_("[-- The following data is signed --]\n\n"), s);

      FREE(&signatures);
    }
    else
      state_attach_puts(_("[-- Warning: Can't find any signatures. --]\n\n"), s);
  }

  rc = mutt_body_handler(a, s);

  if (s->flags & MUTT_DISPLAY && sigcnt)
    state_attach_puts(_("\n[-- End of signed data --]\n"), s);

  return rc;
}

/**
 * crypt_get_fingerprint_or_id - Get the fingerprint or long key ID
 * @param p       String to examine
 * @param pphint  Start of string to be passed to pgp_add_string_to_hints() or
 *                crypt_add_string_to_hints()
 * @param ppl     Start of long key ID if detected, else NULL
 * @param pps     Start of short key ID if detected, else NULL
 * @retval  string Copy of fingerprint, if any, stripped of all spaces
 *                 Must be FREE'd by caller
 * @retval  NULL   Otherwise
 *
 * Obtain pointers to fingerprint or short or long key ID, if any.
 *
 * Upon return, at most one of return, *ppl and *pps pointers is non-NULL,
 * indicating the longest fingerprint or ID found, if any.
 */
const char *crypt_get_fingerprint_or_id(char *p, const char **pphint,
                                        const char **ppl, const char **pps)
{
  const char *ps = NULL, *pl = NULL, *phint = NULL;
  char *pfcopy = NULL, *pf = NULL, *s1 = NULL, *s2 = NULL;
  char c;
  int isid;
  size_t hexdigits;

  /* User input may be partial name, fingerprint or short or long key ID,
   * independent of OPT_PGP_LONG_IDS.
   * Fingerprint without spaces is 40 hex digits (SHA-1) or 32 hex digits (MD5).
   * Strip leading "0x" for key ID detection and prepare pl and ps to indicate
   * if an ID was found and to simplify logic in the key loop's inner
   * condition of the caller. */

  pf = mutt_skip_whitespace(p);
  if (mutt_strncasecmp(pf, "0x", 2) == 0)
    pf += 2;

  /* Check if a fingerprint is given, must be hex digits only, blanks
   * separating groups of 4 hex digits are allowed. Also pre-check for ID. */
  isid = 2; /* unknown */
  hexdigits = 0;
  s1 = pf;
  do
  {
    c = *(s1++);
    if (('0' <= c && c <= '9') || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f'))
    {
      hexdigits++;
      if (isid == 2)
        isid = 1; /* it is an ID so far */
    }
    else if (c)
    {
      isid = 0; /* not an ID */
      if (c == ' ' && ((hexdigits % 4) == 0))
        ; /* skip blank before or after 4 hex digits */
      else
        break; /* any other character or position */
    }
  } while (c);

  /* If at end of input, check for correct fingerprint length and copy if. */
  pfcopy = (!c && ((hexdigits == 40) || (hexdigits == 32)) ? safe_strdup(pf) : NULL);

  if (pfcopy)
  {
    /* Use pfcopy to strip all spaces from fingerprint and as hint. */
    s1 = s2 = pfcopy;
    do
    {
      *(s1++) = *(s2 = mutt_skip_whitespace(s2));
    } while (*(s2++));

    phint = pfcopy;
    ps = pl = NULL;
  }
  else
  {
    phint = p;
    ps = pl = NULL;
    if (isid == 1)
    {
      if (mutt_strlen(pf) == 16)
        pl = pf; /* long key ID */
      else if (mutt_strlen(pf) == 8)
        ps = pf; /* short key ID */
    }
  }

  *pphint = phint;
  *ppl = pl;
  *pps = ps;
  return pfcopy;
}

/**
 * crypt_is_numerical_keyid - Is this a numerical keyid
 *
 * Check if a crypt-hook value is a key id.
 */
bool crypt_is_numerical_keyid(const char *s)
{
  /* or should we require the "0x"? */
  if (strncmp(s, "0x", 2) == 0)
    s += 2;
  if (strlen(s) % 8)
    return false;
  while (*s)
    if (strchr("0123456789ABCDEFabcdef", *s++) == NULL)
      return false;

  return true;
}
