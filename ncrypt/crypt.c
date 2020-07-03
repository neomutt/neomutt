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
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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

/**
 * @page crypt_crypt Signing/encryption multiplexor
 *
 * Signing/encryption multiplexor
 */

#include "config.h"
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "crypt.h"
#include "copy.h"
#include "cryptglue.h"
#include "handler.h"
#include "init.h"
#include "mutt_globals.h"
#include "mutt_parse.h"
#include "muttlib.h"
#include "options.h"
#include "state.h"
#include "ncrypt/lib.h"
#include "send/lib.h"
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif

struct Mailbox;

/* These Config Variables are only used in ncrypt/crypt.c */
bool C_CryptTimestamp; ///< Config: Add a timestamp to PGP or SMIME output to prevent spoofing
unsigned char C_PgpEncryptSelf;
unsigned char C_PgpMimeAuto; ///< Config: Prompt the user to use MIME if inline PGP fails
bool C_PgpRetainableSigs; ///< Config: Create nested multipart/signed or encrypted messages
bool C_PgpSelfEncrypt; ///< Config: Encrypted messages will also be encrypted to C_PgpDefaultKey too
bool C_PgpStrictEnc; ///< Config: Encode PGP signed messages with quoted-printable (don't unset)
unsigned char C_SmimeEncryptSelf;
bool C_SmimeSelfEncrypt; ///< Config: Encrypted messages will also be encrypt to C_SmimeDefaultKey too

/**
 * crypt_current_time - Print the current time
 * @param s        State to use
 * @param app_name App name, e.g. "PGP"
 *
 * print the current time to avoid spoofing of the signature output
 */
void crypt_current_time(struct State *s, const char *app_name)
{
  char p[256], tmp[256];

  if (!WithCrypto)
    return;

  if (C_CryptTimestamp)
  {
    mutt_date_localtime_format(p, sizeof(p), _(" (current time: %c)"), MUTT_DATE_NOW);
  }
  else
    *p = '\0';

  snprintf(tmp, sizeof(tmp), _("[-- %s output follows%s --]\n"), NONULL(app_name), p);
  state_attach_puts(s, tmp);
}

/**
 * crypt_forget_passphrase - Forget a passphrase and display a message
 */
void crypt_forget_passphrase(void)
{
  if (WithCrypto & APPLICATION_PGP)
    crypt_pgp_void_passphrase();

  if (WithCrypto & APPLICATION_SMIME)
    crypt_smime_void_passphrase();

  if (WithCrypto)
  {
    /* L10N: Due to the implementation details (e.g. some passwords are managed
       by gpg-agent) we can't know whether we forgot zero, 1, 12, ...
       passwords. So in English we use "Passphrases". Your language might
       have other means to express this. */
    mutt_message(_("Passphrases forgotten"));
  }
}

#ifndef DEBUG
#include <sys/resource.h>
/**
 * disable_coredumps - Prevent coredumps if neomutt were to crash
 */
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
 * @param flags Flags, see #SecurityFlags
 * @retval true  Success
 * @retval false Failed
 */
bool crypt_valid_passphrase(SecurityFlags flags)
{
  bool rc = false;

#ifndef DEBUG
  disable_coredumps();
#endif

  if (((WithCrypto & APPLICATION_PGP) != 0) && (flags & APPLICATION_PGP))
    rc = crypt_pgp_valid_passphrase();

  if (((WithCrypto & APPLICATION_SMIME) != 0) && (flags & APPLICATION_SMIME))
    rc = crypt_smime_valid_passphrase();

  return rc;
}

/**
 * mutt_protect - Encrypt and/or sign a message
 * @param e        Email
 * @param keylist  List of keys to encrypt to (space-separated)
 * @param postpone When true, signing is automatically disabled
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_protect(struct Email *e, char *keylist, bool postpone)
{
  struct Body *pbody = NULL, *tmp_pbody = NULL;
  struct Body *tmp_smime_pbody = NULL;
  struct Body *tmp_pgp_pbody = NULL;
  bool has_retainable_sig = false;

  if (!WithCrypto)
    return -1;

  int security = e->security;
  int sign = security & (SEC_AUTOCRYPT | SEC_SIGN);
  if (postpone)
  {
    sign = SEC_NO_FLAGS;
    security &= ~SEC_SIGN;
  }

  if (!(security & (SEC_ENCRYPT | SEC_AUTOCRYPT)) && !sign)
    return 0;

  if (sign && !(security & SEC_AUTOCRYPT) && !crypt_valid_passphrase(security))
    return -1;

  if (((WithCrypto & APPLICATION_PGP) != 0) && !(security & SEC_AUTOCRYPT) &&
      ((security & PGP_INLINE) == PGP_INLINE))
  {
    if ((e->content->type != TYPE_TEXT) || !mutt_istr_equal(e->content->subtype, "plain"))
    {
      if (query_quadoption(C_PgpMimeAuto,
                           _("Inline PGP can't be used with attachments.  "
                             "Revert to PGP/MIME?")) != MUTT_YES)
      {
        mutt_error(
            _("Mail not sent: inline PGP can't be used with attachments"));
        return -1;
      }
    }
    else if (mutt_istr_equal("flowed", mutt_param_get(&e->content->parameter, "format")))
    {
      if ((query_quadoption(C_PgpMimeAuto,
                            _("Inline PGP can't be used with format=flowed.  "
                              "Revert to PGP/MIME?"))) != MUTT_YES)
      {
        mutt_error(
            _("Mail not sent: inline PGP can't be used with format=flowed"));
        return -1;
      }
    }
    else
    {
      /* they really want to send it inline... go for it */
      if (!isendwin())
      {
        mutt_endwin();
        puts(_("Invoking PGP..."));
      }
      pbody = crypt_pgp_traditional_encryptsign(e->content, security, keylist);
      if (pbody)
      {
        e->content = pbody;
        return 0;
      }

      /* otherwise inline won't work...ask for revert */
      if (query_quadoption(
              C_PgpMimeAuto,
              _("Message can't be sent inline.  Revert to using PGP/MIME?")) != MUTT_YES)
      {
        mutt_error(_("Mail not sent"));
        return -1;
      }
    }

    /* go ahead with PGP/MIME */
  }

  if (!isendwin())
    mutt_endwin();

  if (WithCrypto & APPLICATION_SMIME)
    tmp_smime_pbody = e->content;
  if (WithCrypto & APPLICATION_PGP)
    tmp_pgp_pbody = e->content;

#ifdef CRYPT_BACKEND_GPGME
  if (sign && C_CryptUsePka)
#else
  if (sign)
#endif
  {
    /* Set sender (necessary for e.g. PKA).  */
    const char *mailbox = NULL;
    struct Address *from = TAILQ_FIRST(&e->env->from);
    bool free_from = false;

    if (!from)
    {
      free_from = true;
      from = mutt_default_from(NeoMutt->sub);
    }

    mailbox = from->mailbox;
    if (!mailbox && C_EnvelopeFromAddress)
      mailbox = C_EnvelopeFromAddress->mailbox;

    if (((WithCrypto & APPLICATION_SMIME) != 0) && (security & APPLICATION_SMIME))
      crypt_smime_set_sender(mailbox);
    else if (((WithCrypto & APPLICATION_PGP) != 0) && (security & APPLICATION_PGP))
      crypt_pgp_set_sender(mailbox);

    if (free_from)
      mutt_addr_free(&from);
  }

  if (C_CryptProtectedHeadersWrite)
  {
    struct Envelope *protected_headers = mutt_env_new();
    mutt_str_replace(&protected_headers->subject, e->env->subject);
    /* Note: if other headers get added, such as to, cc, then a call to
     * mutt_env_to_intl() will need to be added here too. */
    mutt_prepare_envelope(protected_headers, 0, NeoMutt->sub);

    mutt_env_free(&e->content->mime_headers);
    e->content->mime_headers = protected_headers;
    /* Optional part of the draft RFC, but required by Enigmail */
    mutt_param_set(&e->content->parameter, "protected-headers", "v1");
  }
  else
  {
    mutt_param_delete(&e->content->parameter, "protected-headers");
  }

#ifdef USE_AUTOCRYPT
  /* A note about e->content->mime_headers.  If postpone or send
   * fails, the mime_headers is cleared out before returning to the
   * compose menu.  So despite the "robustness" code above and in the
   * gen_gossip_list function below, mime_headers will not be set when
   * entering mutt_protect().
   *
   * This is important to note because the user could toggle
   * $crypt_protected_headers_write or $autocrypt off back in the
   * compose menu.  We don't want mutt_write_rfc822_header() to write
   * stale data from one option if the other is set.
   */
  if (C_Autocrypt && !postpone && (security & SEC_AUTOCRYPT))
  {
    mutt_autocrypt_generate_gossip_list(e);
  }
#endif

  if (sign)
  {
    if (((WithCrypto & APPLICATION_SMIME) != 0) && (security & APPLICATION_SMIME))
    {
      tmp_pbody = crypt_smime_sign_message(e->content, &e->env->from);
      if (!tmp_pbody)
        goto bail;
      pbody = tmp_pbody;
      tmp_smime_pbody = tmp_pbody;
    }

    if (((WithCrypto & APPLICATION_PGP) != 0) && (security & APPLICATION_PGP) &&
        (!(security & (SEC_ENCRYPT | SEC_AUTOCRYPT)) || C_PgpRetainableSigs))
    {
      tmp_pbody = crypt_pgp_sign_message(e->content, &e->env->from);
      if (!tmp_pbody)
        goto bail;

      has_retainable_sig = true;
      sign = SEC_NO_FLAGS;
      pbody = tmp_pbody;
      tmp_pgp_pbody = tmp_pbody;
    }

    if ((WithCrypto != 0) && (security & APPLICATION_SMIME) && (security & APPLICATION_PGP))
    {
      /* here comes the draft ;-) */
    }
  }

  if (security & (SEC_ENCRYPT | SEC_AUTOCRYPT))
  {
    if (((WithCrypto & APPLICATION_SMIME) != 0) && (security & APPLICATION_SMIME))
    {
      tmp_pbody = crypt_smime_build_smime_entity(tmp_smime_pbody, keylist);
      if (!tmp_pbody)
      {
        /* signed ? free it! */
        goto bail;
      }
      /* free tmp_body if messages was signed AND encrypted ... */
      if ((tmp_smime_pbody != e->content) && (tmp_smime_pbody != tmp_pbody))
      {
        /* detach and don't delete e->content,
         * which tmp_smime_pbody->parts after signing. */
        tmp_smime_pbody->parts = tmp_smime_pbody->parts->next;
        e->content->next = NULL;
        mutt_body_free(&tmp_smime_pbody);
      }
      pbody = tmp_pbody;
    }

    if (((WithCrypto & APPLICATION_PGP) != 0) && (security & APPLICATION_PGP))
    {
      pbody = crypt_pgp_encrypt_message(e, tmp_pgp_pbody, keylist, sign, &e->env->from);
      if (!pbody)
      {
        /* did we perform a retainable signature? */
        if (has_retainable_sig)
        {
          /* remove the outer multipart layer */
          tmp_pgp_pbody = mutt_remove_multipart(tmp_pgp_pbody);
          /* get rid of the signature */
          mutt_body_free(&tmp_pgp_pbody->next);
        }

        goto bail;
      }

      // destroy temporary signature envelope when doing retainable signatures.
      if (has_retainable_sig)
      {
        tmp_pgp_pbody = mutt_remove_multipart(tmp_pgp_pbody);
        mutt_body_free(&tmp_pgp_pbody->next);
      }
    }
  }

  if (pbody)
  {
    e->content = pbody;
    return 0;
  }

bail:
  mutt_env_free(&e->content->mime_headers);
  return -1;
}

/**
 * mutt_is_multipart_signed - Is a message signed?
 * @param b Body of email
 * @retval num Message is signed, see #SecurityFlags
 * @retval   0 Message is not signed (#SEC_NO_FLAGS)
 */
SecurityFlags mutt_is_multipart_signed(struct Body *b)
{
  if (!b || (b->type != TYPE_MULTIPART) || !b->subtype || !mutt_istr_equal(b->subtype, "signed"))
  {
    return SEC_NO_FLAGS;
  }

  char *p = mutt_param_get(&b->parameter, "protocol");
  if (!p)
    return SEC_NO_FLAGS;

  if (mutt_istr_equal(p, "multipart/mixed"))
    return SEC_SIGN;

  if (((WithCrypto & APPLICATION_PGP) != 0) &&
      mutt_istr_equal(p, "application/pgp-signature"))
  {
    return PGP_SIGN;
  }

  if (((WithCrypto & APPLICATION_SMIME) != 0) &&
      mutt_istr_equal(p, "application/x-pkcs7-signature"))
  {
    return SMIME_SIGN;
  }
  if (((WithCrypto & APPLICATION_SMIME) != 0) &&
      mutt_istr_equal(p, "application/pkcs7-signature"))
  {
    return SMIME_SIGN;
  }

  return SEC_NO_FLAGS;
}

/**
 * mutt_is_multipart_encrypted - Does the message have encrypted parts?
 * @param b Body of email
 * @retval num Message has got encrypted parts, see #SecurityFlags
 * @retval   0 Message hasn't got encrypted parts (#SEC_NO_FLAGS)
 */
SecurityFlags mutt_is_multipart_encrypted(struct Body *b)
{
  if ((WithCrypto & APPLICATION_PGP) == 0)
    return SEC_NO_FLAGS;

  char *p = NULL;

  if (!b || (b->type != TYPE_MULTIPART) || !b->subtype ||
      !mutt_istr_equal(b->subtype, "encrypted") ||
      !(p = mutt_param_get(&b->parameter, "protocol")) ||
      !mutt_istr_equal(p, "application/pgp-encrypted"))
  {
    return SEC_NO_FLAGS;
  }

  return PGP_ENCRYPT;
}

/**
 * mutt_is_valid_multipart_pgp_encrypted - Is this a valid multi-part encrypted message?
 * @param b Body of email
 * @retval >0 Message is valid, with encrypted parts, e.g. #PGP_ENCRYPT
 * @retval  0 Message hasn't got encrypted parts
 */
int mutt_is_valid_multipart_pgp_encrypted(struct Body *b)
{
  if (mutt_is_multipart_encrypted(b) == SEC_NO_FLAGS)
    return 0;

  b = b->parts;
  if (!b || (b->type != TYPE_APPLICATION) || !b->subtype ||
      !mutt_istr_equal(b->subtype, "pgp-encrypted"))
  {
    return 0;
  }

  b = b->next;
  if (!b || (b->type != TYPE_APPLICATION) || !b->subtype ||
      !mutt_istr_equal(b->subtype, "octet-stream"))
  {
    return 0;
  }

  return PGP_ENCRYPT;
}

/**
 * mutt_is_malformed_multipart_pgp_encrypted - Check for malformed layout
 * @param b Body of email
 * @retval num Success, see #SecurityFlags
 * @retval   0 Error, (#SEC_NO_FLAGS)
 *
 * This checks for the malformed layout caused by MS Exchange in
 * some cases:
 * ```
 *  <multipart/mixed>
 *     <text/plain>
 *     <application/pgp-encrypted> [BASE64-encoded]
 *     <application/octet-stream> [BASE64-encoded]
 * ```
 */
SecurityFlags mutt_is_malformed_multipart_pgp_encrypted(struct Body *b)
{
  if (!(WithCrypto & APPLICATION_PGP))
    return SEC_NO_FLAGS;

  if (!b || (b->type != TYPE_MULTIPART) || !b->subtype || !mutt_istr_equal(b->subtype, "mixed"))
  {
    return SEC_NO_FLAGS;
  }

  b = b->parts;
  if (!b || (b->type != TYPE_TEXT) || !b->subtype ||
      !mutt_istr_equal(b->subtype, "plain") || (b->length != 0))
  {
    return SEC_NO_FLAGS;
  }

  b = b->next;
  if (!b || (b->type != TYPE_APPLICATION) || !b->subtype ||
      !mutt_istr_equal(b->subtype, "pgp-encrypted"))
  {
    return SEC_NO_FLAGS;
  }

  b = b->next;
  if (!b || (b->type != TYPE_APPLICATION) || !b->subtype ||
      !mutt_istr_equal(b->subtype, "octet-stream"))
  {
    return SEC_NO_FLAGS;
  }

  b = b->next;
  if (b)
    return SEC_NO_FLAGS;

  return PGP_ENCRYPT;
}

/**
 * mutt_is_application_pgp - Does the message use PGP?
 * @param m Body of email
 * @retval >0 Message uses PGP, e.g. #PGP_ENCRYPT
 * @retval  0 Message doesn't use PGP, (#SEC_NO_FLAGS)
 */
SecurityFlags mutt_is_application_pgp(struct Body *m)
{
  SecurityFlags t = SEC_NO_FLAGS;
  char *p = NULL;

  if (m->type == TYPE_APPLICATION)
  {
    if (mutt_istr_equal(m->subtype, "pgp") ||
        mutt_istr_equal(m->subtype, "x-pgp-message"))
    {
      p = mutt_param_get(&m->parameter, "x-action");
      if (p && (mutt_istr_equal(p, "sign") || mutt_istr_equal(p, "signclear")))
      {
        t |= PGP_SIGN;
      }

      p = mutt_param_get(&m->parameter, "format");
      if (p && mutt_istr_equal(p, "keys-only"))
      {
        t |= PGP_KEY;
      }

      if (t == SEC_NO_FLAGS)
        t |= PGP_ENCRYPT; /* not necessarily correct, but... */
    }

    if (mutt_istr_equal(m->subtype, "pgp-signed"))
      t |= PGP_SIGN;

    if (mutt_istr_equal(m->subtype, "pgp-keys"))
      t |= PGP_KEY;
  }
  else if ((m->type == TYPE_TEXT) && mutt_istr_equal("plain", m->subtype))
  {
    if (((p = mutt_param_get(&m->parameter, "x-mutt-action")) ||
         (p = mutt_param_get(&m->parameter, "x-action")) ||
         (p = mutt_param_get(&m->parameter, "action"))) &&
        mutt_istr_startswith(p, "pgp-sign"))
    {
      t |= PGP_SIGN;
    }
    else if (p && mutt_istr_startswith(p, "pgp-encrypt"))
      t |= PGP_ENCRYPT;
    else if (p && mutt_istr_startswith(p, "pgp-keys"))
      t |= PGP_KEY;
  }
  if (t)
    t |= PGP_INLINE;

  return t;
}

/**
 * mutt_is_application_smime - Does the message use S/MIME?
 * @param m Body of email
 * @retval >0 Message uses S/MIME, e.g. #SMIME_ENCRYPT
 * @retval  0 Message doesn't use S/MIME, (#SEC_NO_FLAGS)
 */
SecurityFlags mutt_is_application_smime(struct Body *m)
{
  if (!m)
    return SEC_NO_FLAGS;

  if (((m->type & TYPE_APPLICATION) == 0) || !m->subtype)
    return SEC_NO_FLAGS;

  char *t = NULL;
  bool complain = false;
  /* S/MIME MIME types don't need x- anymore, see RFC2311 */
  if (mutt_istr_equal(m->subtype, "x-pkcs7-mime") || mutt_istr_equal(m->subtype, "pkcs7-mime"))
  {
    t = mutt_param_get(&m->parameter, "smime-type");
    if (t)
    {
      if (mutt_istr_equal(t, "enveloped-data"))
        return SMIME_ENCRYPT;
      if (mutt_istr_equal(t, "signed-data"))
        return SMIME_SIGN | SMIME_OPAQUE;
      return SEC_NO_FLAGS;
    }
    /* Netscape 4.7 uses
     * Content-Description: S/MIME Encrypted Message
     * instead of Content-Type parameter */
    if (mutt_istr_equal(m->description, "S/MIME Encrypted Message"))
      return SMIME_ENCRYPT;
    complain = true;
  }
  else if (!mutt_istr_equal(m->subtype, "octet-stream"))
    return SEC_NO_FLAGS;

  t = mutt_param_get(&m->parameter, "name");

  if (!t)
    t = m->d_filename;
  if (!t)
    t = m->filename;
  if (!t)
  {
    if (complain)
    {
      mutt_message(
          _("S/MIME messages with no hints on content are unsupported"));
    }
    return SEC_NO_FLAGS;
  }

  /* no .p7c, .p10 support yet. */

  int len = mutt_str_len(t) - 4;
  if ((len > 0) && (*(t + len) == '.'))
  {
    len++;
    if (mutt_istr_equal((t + len), "p7m"))
    {
      /* Not sure if this is the correct thing to do, but
       * it's required for compatibility with Outlook */
      return SMIME_SIGN | SMIME_OPAQUE;
    }
    else if (mutt_istr_equal((t + len), "p7s"))
      return SMIME_SIGN | SMIME_OPAQUE;
  }

  return SEC_NO_FLAGS;
}

/**
 * crypt_query - Check out the type of encryption used
 * @param m Body of email
 * @retval num Flags, see #SecurityFlags
 * @retval 0   Error (#SEC_NO_FLAGS)
 *
 * Set the cached status values if there are any.
 */
SecurityFlags crypt_query(struct Body *m)
{
  if (!WithCrypto || !m)
    return SEC_NO_FLAGS;

  SecurityFlags rc = SEC_NO_FLAGS;

  if (m->type == TYPE_APPLICATION)
  {
    if (WithCrypto & APPLICATION_PGP)
      rc |= mutt_is_application_pgp(m);

    if (WithCrypto & APPLICATION_SMIME)
    {
      rc |= mutt_is_application_smime(m);
      if (rc && m->goodsig)
        rc |= SEC_GOODSIGN;
      if (rc && m->badsig)
        rc |= SEC_BADSIGN;
    }
  }
  else if (((WithCrypto & APPLICATION_PGP) != 0) && (m->type == TYPE_TEXT))
  {
    rc |= mutt_is_application_pgp(m);
    if (rc && m->goodsig)
      rc |= SEC_GOODSIGN;
  }

  if (m->type == TYPE_MULTIPART)
  {
    rc |= mutt_is_multipart_encrypted(m);
    rc |= mutt_is_multipart_signed(m);
    rc |= mutt_is_malformed_multipart_pgp_encrypted(m);

    if (rc && m->goodsig)
      rc |= SEC_GOODSIGN;
#ifdef USE_AUTOCRYPT
    if (rc && m->is_autocrypt)
      rc |= SEC_AUTOCRYPT;
#endif
  }

  if ((m->type == TYPE_MULTIPART) || (m->type == TYPE_MESSAGE))
  {
    SecurityFlags u = m->parts ? SEC_ALL_FLAGS : SEC_NO_FLAGS; /* Bits set in all parts */
    SecurityFlags w = SEC_NO_FLAGS; /* Bits set in any part  */

    for (struct Body *b = m->parts; b; b = b->next)
    {
      const SecurityFlags v = crypt_query(b);
      u &= v;
      w |= v;
    }
    rc |= u | (w & ~SEC_GOODSIGN);

    if ((w & SEC_GOODSIGN) && !(u & SEC_GOODSIGN))
      rc |= SEC_PARTSIGN;
  }

  return rc;
}

/**
 * crypt_write_signed - Write the message body/part
 * @param a        Body to write
 * @param s        State to use
 * @param tempfile File to write to
 * @retval  0 Success
 * @retval -1 Error
 *
 * Body/part A described by state S to the given TEMPFILE.
 */
int crypt_write_signed(struct Body *a, struct State *s, const char *tempfile)
{
  if (!WithCrypto)
    return -1;

  FILE *fp = mutt_file_fopen(tempfile, "w");
  if (!fp)
  {
    mutt_perror(tempfile);
    return -1;
  }

  fseeko(s->fp_in, a->hdr_offset, SEEK_SET);
  size_t bytes = a->length + a->offset - a->hdr_offset;
  bool hadcr = false;
  while (bytes > 0)
  {
    const int c = fgetc(s->fp_in);
    if (c == EOF)
      break;

    bytes--;

    if (c == '\r')
      hadcr = true;
    else
    {
      if ((c == '\n') && !hadcr)
        fputc('\r', fp);

      hadcr = false;
    }

    fputc(c, fp);
  }
  mutt_file_fclose(&fp);

  return 0;
}

/**
 * crypt_convert_to_7bit - Convert an email to 7bit encoding
 * @param a Body of email to convert
 */
void crypt_convert_to_7bit(struct Body *a)
{
  if (!WithCrypto)
    return;

  while (a)
  {
    if (a->type == TYPE_MULTIPART)
    {
      if (a->encoding != ENC_7BIT)
      {
        a->encoding = ENC_7BIT;
        crypt_convert_to_7bit(a->parts);
      }
      else if (((WithCrypto & APPLICATION_PGP) != 0) && C_PgpStrictEnc)
        crypt_convert_to_7bit(a->parts);
    }
    else if ((a->type == TYPE_MESSAGE) &&
             !mutt_istr_equal(a->subtype, "delivery-status"))
    {
      if (a->encoding != ENC_7BIT)
        mutt_message_to_7bit(a, NULL, NeoMutt->sub);
    }
    else if (a->encoding == ENC_8BIT)
      a->encoding = ENC_QUOTED_PRINTABLE;
    else if (a->encoding == ENC_BINARY)
      a->encoding = ENC_BASE64;
    else if (a->content && (a->encoding != ENC_BASE64) &&
             (a->content->from || (a->content->space && C_PgpStrictEnc)))
    {
      a->encoding = ENC_QUOTED_PRINTABLE;
    }
    a = a->next;
  }
}

/**
 * crypt_extract_keys_from_messages - Extract keys from a message
 * @param m  Mailbox
 * @param el List of Emails to process
 *
 * The extracted keys will be added to the user's keyring.
 */
void crypt_extract_keys_from_messages(struct Mailbox *m, struct EmailList *el)
{
  if (!WithCrypto)
    return;

  struct Buffer *tempfname = mutt_buffer_pool_get();
  mutt_buffer_mktemp(tempfname);
  FILE *fp_out = mutt_file_fopen(mutt_b2s(tempfname), "w");
  if (!fp_out)
  {
    mutt_perror(mutt_b2s(tempfname));
    goto cleanup;
  }

  if (WithCrypto & APPLICATION_PGP)
    OptDontHandlePgpKeys = true;

  struct EmailNode *en = NULL;
  STAILQ_FOREACH(en, el, entries)
  {
    struct Email *e = en->email;

    mutt_parse_mime_message(m, e);
    if (e->security & SEC_ENCRYPT && !crypt_valid_passphrase(e->security))
    {
      mutt_file_fclose(&fp_out);
      break;
    }

    if (((WithCrypto & APPLICATION_PGP) != 0) && (e->security & APPLICATION_PGP))
    {
      mutt_copy_message(fp_out, m, e, MUTT_CM_DECODE | MUTT_CM_CHARCONV, CH_NO_FLAGS, 0);
      fflush(fp_out);

      mutt_endwin();
      puts(_("Trying to extract PGP keys...\n"));
      crypt_pgp_invoke_import(mutt_b2s(tempfname));
    }

    if (((WithCrypto & APPLICATION_SMIME) != 0) && (e->security & APPLICATION_SMIME))
    {
      if (e->security & SEC_ENCRYPT)
      {
        mutt_copy_message(fp_out, m, e, MUTT_CM_NOHEADER | MUTT_CM_DECODE_CRYPT | MUTT_CM_DECODE_SMIME,
                          CH_NO_FLAGS, 0);
      }
      else
        mutt_copy_message(fp_out, m, e, MUTT_CM_NO_FLAGS, CH_NO_FLAGS, 0);
      fflush(fp_out);

      char *mbox = NULL;
      if (!TAILQ_EMPTY(&e->env->from))
      {
        mutt_expand_aliases(&e->env->from);
        mbox = TAILQ_FIRST(&e->env->from)->mailbox;
      }
      else if (!TAILQ_EMPTY(&e->env->sender))
      {
        mutt_expand_aliases(&e->env->sender);
        mbox = TAILQ_FIRST(&e->env->sender)->mailbox;
      }
      if (mbox)
      {
        mutt_endwin();
        puts(_("Trying to extract S/MIME certificates..."));
        crypt_smime_invoke_import(mutt_b2s(tempfname), mbox);
      }
    }

    rewind(fp_out);
  }

  mutt_file_fclose(&fp_out);
  if (isendwin())
    mutt_any_key_to_continue(NULL);

  mutt_file_unlink(mutt_b2s(tempfname));

  if (WithCrypto & APPLICATION_PGP)
    OptDontHandlePgpKeys = false;

cleanup:
  mutt_buffer_pool_release(&tempfname);
}

/**
 * crypt_get_keys - Check we have all the keys we need
 * @param[in]  e           Email with addresses to match
 * @param[out] keylist     Keys needed
 * @param[in]  oppenc_mode If true, use opportunistic encryption
 * @retval  0 Success
 * @retval -1 Error
 *
 * Do a quick check to make sure that we can find all of the
 * encryption keys if the user has requested this service.
 * Return the list of keys in KEYLIST.
 * If oppenc_mode is true, only keys that can be determined without
 * prompting will be used.
 */
int crypt_get_keys(struct Email *e, char **keylist, bool oppenc_mode)
{
  if (!WithCrypto)
    return 0;

  struct AddressList addrlist = TAILQ_HEAD_INITIALIZER(addrlist);
  const char *fqdn = mutt_fqdn(true, NeoMutt->sub);
  char *self_encrypt = NULL;

  /* Do a quick check to make sure that we can find all of the encryption
   * keys if the user has requested this service.  */

  *keylist = NULL;

#ifdef USE_AUTOCRYPT
  if (!oppenc_mode && (e->security & SEC_AUTOCRYPT))
  {
    if (mutt_autocrypt_ui_recommendation(e, keylist) <= AUTOCRYPT_REC_NO)
      return -1;
    return 0;
  }
#endif

  if (WithCrypto & APPLICATION_PGP)
    OptPgpCheckTrust = true;

  mutt_addrlist_copy(&addrlist, &e->env->to, false);
  mutt_addrlist_copy(&addrlist, &e->env->cc, false);
  mutt_addrlist_copy(&addrlist, &e->env->bcc, false);
  mutt_addrlist_qualify(&addrlist, fqdn);
  mutt_addrlist_dedupe(&addrlist);

  if (oppenc_mode || (e->security & SEC_ENCRYPT))
  {
    if (((WithCrypto & APPLICATION_PGP) != 0) && (e->security & APPLICATION_PGP))
    {
      *keylist = crypt_pgp_find_keys(&addrlist, oppenc_mode);
      if (!*keylist)
      {
        mutt_addrlist_clear(&addrlist);
        return -1;
      }
      OptPgpCheckTrust = false;
      if (C_PgpSelfEncrypt || (C_PgpEncryptSelf == MUTT_YES))
        self_encrypt = C_PgpDefaultKey;
    }
    if (((WithCrypto & APPLICATION_SMIME) != 0) && (e->security & APPLICATION_SMIME))
    {
      *keylist = crypt_smime_find_keys(&addrlist, oppenc_mode);
      if (!*keylist)
      {
        mutt_addrlist_clear(&addrlist);
        return -1;
      }
      if (C_SmimeSelfEncrypt || (C_SmimeEncryptSelf == MUTT_YES))
        self_encrypt = C_SmimeDefaultKey;
    }
  }

  if (!oppenc_mode && self_encrypt)
  {
    const size_t keylist_size = mutt_str_len(*keylist);
    mutt_mem_realloc(keylist, keylist_size + mutt_str_len(self_encrypt) + 2);
    sprintf(*keylist + keylist_size, " %s", self_encrypt);
  }

  mutt_addrlist_clear(&addrlist);

  return 0;
}

/**
 * crypt_opportunistic_encrypt - Can all recipients be determined
 * @param e Email
 *
 * Check if all recipients keys can be automatically determined.
 * Enable encryption if they can, otherwise disable encryption.
 */
void crypt_opportunistic_encrypt(struct Email *e)
{
  if (!WithCrypto)
    return;

  if (!(C_CryptOpportunisticEncrypt && (e->security & SEC_OPPENCRYPT)))
    return;

  char *pgpkeylist = NULL;

  crypt_get_keys(e, &pgpkeylist, 1);
  if (pgpkeylist)
  {
    e->security |= SEC_ENCRYPT;
    FREE(&pgpkeylist);
  }
  else
  {
    e->security &= ~SEC_ENCRYPT;
  }
}

/**
 * crypt_fetch_signatures - Create an array of an emails parts
 * @param[out] signatures Array of Body parts
 * @param[in]  a          Body part to examine
 * @param[out] n          Cumulative count of parts
 */
static void crypt_fetch_signatures(struct Body ***signatures, struct Body *a, int *n)
{
  if (!WithCrypto)
    return;

  for (; a; a = a->next)
  {
    if (a->type == TYPE_MULTIPART)
      crypt_fetch_signatures(signatures, a->parts, n);
    else
    {
      if ((*n % 5) == 0)
        mutt_mem_realloc(signatures, (*n + 6) * sizeof(struct Body **));

      (*signatures)[(*n)++] = a;
    }
  }
}

/**
 * mutt_should_hide_protected_subject - Should NeoMutt hide the protected subject?
 * @param e Email to test
 * @retval bool True if the subject should be protected
 */
bool mutt_should_hide_protected_subject(struct Email *e)
{
  if (C_CryptProtectedHeadersWrite && (e->security & (SEC_ENCRYPT | SEC_AUTOCRYPT)) &&
      !(e->security & SEC_INLINE) && C_CryptProtectedHeadersSubject)
  {
    return true;
  }

  return false;
}

/**
 * mutt_protected_headers_handler - Process a protected header - Implements ::handler_t
 */
int mutt_protected_headers_handler(struct Body *a, struct State *s)
{
  if (C_CryptProtectedHeadersRead && a->mime_headers)
  {
    if (a->mime_headers->subject)
    {
      const bool display = (s->flags & MUTT_DISPLAY);

      if (display && C_Weed && mutt_matches_ignore("subject"))
        return 0;

      state_mark_protected_header(s);
      int wraplen = display ? mutt_window_wrap_cols(s->wraplen, C_Wrap) : 0;

      mutt_write_one_header(s->fp_out, "Subject", a->mime_headers->subject, s->prefix,
                            wraplen, display ? CH_DISPLAY : CH_NO_FLAGS, NeoMutt->sub);
      state_puts(s, "\n");
    }
  }

  return 0;
}

/**
 * mutt_signed_handler - Verify a "multipart/signed" body - Implements ::handler_t
 */
int mutt_signed_handler(struct Body *a, struct State *s)
{
  if (!WithCrypto)
    return -1;

  bool inconsistent = false;
  struct Body *b = a;
  struct Body **signatures = NULL;
  int sigcnt = 0;
  int rc = 0;
  struct Buffer *tempfile = NULL;

  a = a->parts;
  SecurityFlags signed_type = mutt_is_multipart_signed(b);
  if (signed_type == SEC_NO_FLAGS)
  {
    /* A null protocol value is already checked for in mutt_body_handler() */
    state_printf(s,
                 _("[-- Error: "
                   "Unknown multipart/signed protocol %s --]\n\n"),
                 mutt_param_get(&b->parameter, "protocol"));
    return mutt_body_handler(a, s);
  }

  if (!(a && a->next))
    inconsistent = true;
  else
  {
    switch (signed_type)
    {
      case SEC_SIGN:
        if ((a->next->type != TYPE_MULTIPART) || !mutt_istr_equal(a->next->subtype, "mixed"))
        {
          inconsistent = true;
        }
        break;
      case PGP_SIGN:
        if ((a->next->type != TYPE_APPLICATION) ||
            !mutt_istr_equal(a->next->subtype, "pgp-signature"))
        {
          inconsistent = true;
        }
        break;
      case SMIME_SIGN:
        if ((a->next->type != TYPE_APPLICATION) ||
            (!mutt_istr_equal(a->next->subtype, "x-pkcs7-signature") &&
             !mutt_istr_equal(a->next->subtype, "pkcs7-signature")))
        {
          inconsistent = true;
        }
        break;
      default:
        inconsistent = true;
    }
  }
  if (inconsistent)
  {
    state_attach_puts(s, _("[-- Error: Missing or bad-format multipart/signed "
                           "signature --]\n\n"));
    return mutt_body_handler(a, s);
  }

  if (s->flags & MUTT_DISPLAY)
  {
    crypt_fetch_signatures(&signatures, a->next, &sigcnt);

    if (sigcnt != 0)
    {
      tempfile = mutt_buffer_pool_get();
      mutt_buffer_mktemp(tempfile);
      bool goodsig = true;
      if (crypt_write_signed(a, s, mutt_b2s(tempfile)) == 0)
      {
        for (int i = 0; i < sigcnt; i++)
        {
          if (((WithCrypto & APPLICATION_PGP) != 0) &&
              (signatures[i]->type == TYPE_APPLICATION) &&
              mutt_istr_equal(signatures[i]->subtype, "pgp-signature"))
          {
            if (crypt_pgp_verify_one(signatures[i], s, mutt_b2s(tempfile)) != 0)
              goodsig = false;

            continue;
          }

          if (((WithCrypto & APPLICATION_SMIME) != 0) &&
              (signatures[i]->type == TYPE_APPLICATION) &&
              (mutt_istr_equal(signatures[i]->subtype, "x-pkcs7-signature") ||
               mutt_istr_equal(signatures[i]->subtype, "pkcs7-signature")))
          {
            if (crypt_smime_verify_one(signatures[i], s, mutt_b2s(tempfile)) != 0)
              goodsig = false;

            continue;
          }

          state_printf(s,
                       _("[-- Warning: "
                         "We can't verify %s/%s signatures. --]\n\n"),
                       TYPE(signatures[i]), signatures[i]->subtype);
        }
      }

      mutt_file_unlink(mutt_b2s(tempfile));
      mutt_buffer_pool_release(&tempfile);

      b->goodsig = goodsig;
      b->badsig = !goodsig;

      /* Now display the signed body */
      state_attach_puts(s, _("[-- The following data is signed --]\n\n"));

      mutt_protected_headers_handler(a, s);

      FREE(&signatures);
    }
    else
    {
      state_attach_puts(s,
                        _("[-- Warning: Can't find any signatures. --]\n\n"));
    }
  }

  rc = mutt_body_handler(a, s);

  if ((s->flags & MUTT_DISPLAY) && (sigcnt != 0))
    state_attach_puts(s, _("\n[-- End of signed data --]\n"));

  return rc;
}

/**
 * crypt_get_fingerprint_or_id - Get the fingerprint or long key ID
 * @param[in]  p       String to examine
 * @param[out] pphint  Start of string to be passed to pgp_add_string_to_hints() or crypt_add_string_to_hints()
 * @param[out] ppl     Start of long key ID if detected, else NULL
 * @param[out] pps     Start of short key ID if detected, else NULL
 * @retval ptr  Copy of fingerprint, if any, stripped of all spaces.  Must be FREE'd by caller
 * @retval NULL Otherwise
 *
 * Obtain pointers to fingerprint or short or long key ID, if any.
 *
 * Upon return, at most one of return, *ppl and *pps pointers is non-NULL,
 * indicating the longest fingerprint or ID found, if any.
 */
const char *crypt_get_fingerprint_or_id(const char *p, const char **pphint,
                                        const char **ppl, const char **pps)
{
  const char *ps = NULL, *pl = NULL, *phint = NULL;
  char *pfcopy = NULL, *s1 = NULL, *s2 = NULL;
  char c;
  int isid;
  size_t hexdigits;

  /* User input may be partial name, fingerprint or short or long key ID,
   * independent of C_PgpLongIds.
   * Fingerprint without spaces is 40 hex digits (SHA-1) or 32 hex digits (MD5).
   * Strip leading "0x" for key ID detection and prepare pl and ps to indicate
   * if an ID was found and to simplify logic in the key loop's inner
   * condition of the caller. */

  char *pf = mutt_str_skip_whitespace(p);
  if (mutt_istr_startswith(pf, "0x"))
    pf += 2;

  /* Check if a fingerprint is given, must be hex digits only, blanks
   * separating groups of 4 hex digits are allowed. Also pre-check for ID. */
  isid = 2; /* unknown */
  hexdigits = 0;
  s1 = pf;
  do
  {
    c = *(s1++);
    if ((('0' <= c) && (c <= '9')) || (('A' <= c) && (c <= 'F')) ||
        (('a' <= c) && (c <= 'f')))
    {
      hexdigits++;
      if (isid == 2)
        isid = 1; /* it is an ID so far */
    }
    else if (c)
    {
      isid = 0; /* not an ID */
      if ((c == ' ') && ((hexdigits % 4) == 0))
        ; /* skip blank before or after 4 hex digits */
      else
        break; /* any other character or position */
    }
  } while (c);

  /* If at end of input, check for correct fingerprint length and copy if. */
  pfcopy = (!c && ((hexdigits == 40) || (hexdigits == 32)) ? mutt_str_dup(pf) : NULL);

  if (pfcopy)
  {
    /* Use pfcopy to strip all spaces from fingerprint and as hint. */
    s1 = pfcopy;
    s2 = pfcopy;
    do
    {
      *(s1++) = *(s2 = mutt_str_skip_whitespace(s2));
    } while (*(s2++));

    phint = pfcopy;
    ps = NULL;
    pl = NULL;
  }
  else
  {
    phint = p;
    ps = NULL;
    pl = NULL;
    if (isid == 1)
    {
      if (mutt_str_len(pf) == 16)
        pl = pf; /* long key ID */
      else if (mutt_str_len(pf) == 8)
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
 * @param s Key to test
 * @retval true If keyid is numeric
 *
 * Check if a crypt-hook value is a key id.
 */
bool crypt_is_numerical_keyid(const char *s)
{
  /* or should we require the "0x"? */
  if (mutt_strn_equal(s, "0x", 2))
    s += 2;
  if (strlen(s) % 8)
    return false;
  while (*s)
    if (!strchr("0123456789ABCDEFabcdef", *s++))
      return false;

  return true;
}
