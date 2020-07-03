/**
 * @file
 * PGP sign, encrypt, check routines
 *
 * @authors
 * Copyright (C) 1996-1997,2000,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1998-2005 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2004 g10 Code GmbH
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
 * @page crypt_pgp PGP sign, encrypt, check routines
 *
 * This file contains all of the PGP routines necessary to sign, encrypt,
 * verify and decrypt PGP messages in either the new PGP/MIME format, or in
 * the older Application/Pgp format.  It also contains some code to cache the
 * user's passphrase for repeat use when decrypting or signing a message.
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "crypt.h"
#include "cryptglue.h"
#include "handler.h"
#include "hook.h"
#include "mutt_attach.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "options.h"
#include "pgpinvoke.h"
#include "pgpkey.h"
#include "pgpmicalg.h"
#include "state.h"
#include "send/lib.h"
#ifdef CRYPT_BACKEND_CLASSIC_PGP
#include "pgp.h"
#include "pgplib.h"
#endif

/* These Config Variables are only used in ncrypt/pgp.c */
bool C_PgpCheckExit; ///< Config: Check the exit code of PGP subprocess
bool C_PgpCheckGpgDecryptStatusFd; ///< Config: File descriptor used for status info
struct Regex *C_PgpDecryptionOkay; ///< Config: Text indicating a successful decryption
struct Regex *C_PgpGoodSign; ///< Config: Text indicating a good signature
long C_PgpTimeout;           ///< Config: Time in seconds to cache a passphrase
bool C_PgpUseGpgAgent;       ///< Config: Use a PGP agent for caching passwords

char PgpPass[1024];
time_t PgpExptime = 0; /* when does the cached passphrase expire? */

/**
 * pgp_class_void_passphrase - Implements CryptModuleSpecs::void_passphrase()
 */
void pgp_class_void_passphrase(void)
{
  memset(PgpPass, 0, sizeof(PgpPass));
  PgpExptime = 0;
}

/**
 * pgp_class_valid_passphrase - Implements CryptModuleSpecs::valid_passphrase()
 */
bool pgp_class_valid_passphrase(void)
{
  if (pgp_use_gpg_agent())
  {
    *PgpPass = '\0';
    return true; /* handled by gpg-agent */
  }

  if (mutt_date_epoch() < PgpExptime)
  {
    /* Use cached copy.  */
    return true;
  }

  pgp_class_void_passphrase();

  if (mutt_get_password(_("Enter PGP passphrase:"), PgpPass, sizeof(PgpPass)) == 0)
  {
    PgpExptime = mutt_date_add_timeout(mutt_date_epoch(), C_PgpTimeout);
    return true;
  }
  else
    PgpExptime = 0;

  return false;
}

/**
 * pgp_use_gpg_agent - Does the user want to use the gpg agent?
 * @retval true If they do
 *
 * @note This functions sets the environment variable `$GPG_TTY`
 */
bool pgp_use_gpg_agent(void)
{
  char *tty = NULL;

  /* GnuPG 2.1 no longer exports GPG_AGENT_INFO */
  if (!C_PgpUseGpgAgent)
    return false;

  tty = ttyname(0);
  if (tty)
  {
    setenv("GPG_TTY", tty, 0);
    mutt_envlist_set("GPG_TTY", tty, false);
  }

  return true;
}

/**
 * key_parent - Find a key's parent (if it's a subkey)
 * @param k PGP key
 * @retval ptr Parent key
 */
static struct PgpKeyInfo *key_parent(struct PgpKeyInfo *k)
{
  if ((k->flags & KEYFLAG_SUBKEY) && k->parent && C_PgpIgnoreSubkeys)
    k = k->parent;

  return k;
}

/**
 * pgp_long_keyid - Get a key's long id
 * @param k PGP key
 * @retval ptr Long key id string
 */
char *pgp_long_keyid(struct PgpKeyInfo *k)
{
  k = key_parent(k);

  return k->keyid;
}

/**
 * pgp_short_keyid - Get a key's short id
 * @param k PGP key
 * @retval ptr Short key id string
 */
char *pgp_short_keyid(struct PgpKeyInfo *k)
{
  k = key_parent(k);

  return k->keyid + 8;
}

/**
 * pgp_this_keyid - Get the ID of this key
 * @param k PGP key
 * @retval ptr Long/Short key id string
 *
 * @note The string returned depends on `$pgp_long_ids`
 */
char *pgp_this_keyid(struct PgpKeyInfo *k)
{
  if (C_PgpLongIds)
    return k->keyid;
  return k->keyid + 8;
}

/**
 * pgp_keyid - Get the ID of the main (parent) key
 * @param k PGP key
 * @retval ptr Long/Short key id string
 */
char *pgp_keyid(struct PgpKeyInfo *k)
{
  k = key_parent(k);

  return pgp_this_keyid(k);
}

/**
 * pgp_fingerprint - Get the key's fingerprint
 * @param k PGP key
 * @retval ptr Fingerprint string
 */
static char *pgp_fingerprint(struct PgpKeyInfo *k)
{
  k = key_parent(k);

  return k->fingerprint;
}

/**
 * pgp_fpr_or_lkeyid - Get the fingerprint or long keyid
 * @param k PGP key
 * @retval ptr String fingerprint or long keyid
 *
 * Grab the longest key identifier available: fingerprint or else
 * the long keyid.
 *
 * The longest available should be used for internally identifying
 * the key and for invoking pgp commands.
 */
char *pgp_fpr_or_lkeyid(struct PgpKeyInfo *k)
{
  char *fingerprint = pgp_fingerprint(k);
  return fingerprint ? fingerprint : pgp_long_keyid(k);
}

/* ----------------------------------------------------------------------------
 * Routines for handing PGP input.
 */

/**
 * pgp_copy_checksig - Copy PGP output and look for signs of a good signature
 * @param fp_in  File to read from
 * @param fp_out File to write to
 * @retval  0 Success
 * @retval -1 Error
 */
static int pgp_copy_checksig(FILE *fp_in, FILE *fp_out)
{
  if (!fp_in || !fp_out)
    return -1;

  int rc = -1;

  if (C_PgpGoodSign && C_PgpGoodSign->regex)
  {
    char *line = NULL;
    size_t linelen;

    while ((line = mutt_file_read_line(line, &linelen, fp_in, NULL, 0)))
    {
      if (mutt_regex_match(C_PgpGoodSign, line))
      {
        mutt_debug(LL_DEBUG2, "\"%s\" matches regex\n", line);
        rc = 0;
      }
      else
        mutt_debug(LL_DEBUG2, "\"%s\" doesn't match regex\n", line);

      if (mutt_strn_equal(line, "[GNUPG:] ", 9))
        continue;
      fputs(line, fp_out);
      fputc('\n', fp_out);
    }
    FREE(&line);
  }
  else
  {
    mutt_debug(LL_DEBUG2, "No pattern\n");
    mutt_file_copy_stream(fp_in, fp_out);
    rc = 1;
  }

  return rc;
}

/**
 * pgp_check_pgp_decryption_okay_regex - Check PGP output to look for successful outcome
 * @param fp_in File to read from
 * @retval  0 Success
 * @retval -1 Error
 *
 * Checks PGP output messages to look for the $pgp_decryption_okay message.
 * This protects against messages with multipart/encrypted headers but which
 * aren't actually encrypted.
 */
static int pgp_check_pgp_decryption_okay_regex(FILE *fp_in)
{
  int rc = -1;

  if (C_PgpDecryptionOkay && C_PgpDecryptionOkay->regex)
  {
    char *line = NULL;
    size_t linelen;

    while ((line = mutt_file_read_line(line, &linelen, fp_in, NULL, 0)))
    {
      if (mutt_regex_match(C_PgpDecryptionOkay, line))
      {
        mutt_debug(LL_DEBUG2, "\"%s\" matches regex\n", line);
        rc = 0;
        break;
      }
      else
        mutt_debug(LL_DEBUG2, "\"%s\" doesn't match regex\n", line);
    }
    FREE(&line);
  }
  else
  {
    mutt_debug(LL_DEBUG2, "No pattern\n");
    rc = 1;
  }

  return rc;
}

/**
 * pgp_check_decryption_okay - Check GPG output for status codes
 * @param fp_in File to read from
 * @retval  1 - no patterns were matched (if delegated to decryption_okay_regex)
 * @retval  0 - DECRYPTION_OKAY was seen, with no PLAINTEXT outside
 * @retval -1 - No decryption status codes were encountered
 * @retval -2 - PLAINTEXT was encountered outside of DECRYPTION delimiters
 * @retval -3 - DECRYPTION_FAILED was encountered
 *
 * Checks GnuPGP status fd output for various status codes indicating
 * an issue.  If $pgp_check_gpg_decrypt_status_fd is unset, it falls
 * back to the old behavior of just scanning for $pgp_decryption_okay.
 *
 * pgp_decrypt_part() should fail if the part is not encrypted, so we return
 * less than 0 to indicate part or all was NOT actually encrypted.
 *
 * On the other hand, for pgp_application_pgp_handler(), a
 * "BEGIN PGP MESSAGE" could indicate a signed and armored message.
 * For that we allow -1 and -2 as "valid" (with a warning).
 */
static int pgp_check_decryption_okay(FILE *fp_in)
{
  int rc = -1;
  char *line = NULL, *s = NULL;
  size_t linelen;
  int inside_decrypt = 0;

  if (!C_PgpCheckGpgDecryptStatusFd)
    return pgp_check_pgp_decryption_okay_regex(fp_in);

  while ((line = mutt_file_read_line(line, &linelen, fp_in, NULL, 0)))
  {
    size_t plen = mutt_str_startswith(line, "[GNUPG:] ");
    if (plen == 0)
      continue;
    s = line + plen;
    mutt_debug(LL_DEBUG2, "checking \"%s\"\n", line);
    if (mutt_str_startswith(s, "BEGIN_DECRYPTION"))
      inside_decrypt = 1;
    else if (mutt_str_startswith(s, "END_DECRYPTION"))
      inside_decrypt = 0;
    else if (mutt_str_startswith(s, "PLAINTEXT"))
    {
      if (!inside_decrypt)
      {
        mutt_debug(LL_DEBUG2,
                   "\tPLAINTEXT encountered outside of DECRYPTION\n");
        rc = -2;
        break;
      }
    }
    else if (mutt_str_startswith(s, "DECRYPTION_FAILED"))
    {
      mutt_debug(LL_DEBUG2, "\tDECRYPTION_FAILED encountered.  Failure\n");
      rc = -3;
      break;
    }
    else if (mutt_str_startswith(s, "DECRYPTION_OKAY"))
    {
      /* Don't break out because we still have to check for
       * PLAINTEXT outside of the decryption boundaries. */
      mutt_debug(LL_DEBUG2, "\tDECRYPTION_OKAY encountered\n");
      rc = 0;
    }
  }
  FREE(&line);

  return rc;
}

/**
 * pgp_copy_clearsigned - Copy a clearsigned message, stripping the signature
 * @param fp_in    File to read from
 * @param s       State to use
 * @param charset Charset of file
 *
 * XXX charset handling: We assume that it is safe to do character set
 * decoding first, dash decoding second here, while we do it the other way
 * around in the main handler.
 *
 * (Note that we aren't worse than Outlook &c in this, and also note that we
 * can successfully handle anything produced by any existing versions of neomutt.)
 */
static void pgp_copy_clearsigned(FILE *fp_in, struct State *s, char *charset)
{
  char buf[8192];
  bool complete, armor_header;

  rewind(fp_in);

  /* fromcode comes from the MIME Content-Type charset label. It might
   * be a wrong label, so we want the ability to do corrections via
   * charset-hooks. Therefore we set flags to MUTT_ICONV_HOOK_FROM.  */
  struct FgetConv *fc =
      mutt_ch_fgetconv_open(fp_in, charset, C_Charset, MUTT_ICONV_HOOK_FROM);

  for (complete = true, armor_header = true;
       mutt_ch_fgetconvs(buf, sizeof(buf), fc); complete = (strchr(buf, '\n')))
  {
    if (!complete)
    {
      if (!armor_header)
        state_puts(s, buf);
      continue;
    }

    if (mutt_str_equal(buf, "-----BEGIN PGP SIGNATURE-----\n"))
      break;

    if (armor_header)
    {
      char *p = mutt_str_skip_whitespace(buf);
      if (*p == '\0')
        armor_header = false;
      continue;
    }

    if (s->prefix)
      state_puts(s, s->prefix);

    if ((buf[0] == '-') && (buf[1] == ' '))
      state_puts(s, buf + 2);
    else
      state_puts(s, buf);
  }

  mutt_ch_fgetconv_close(&fc);
}

/**
 * pgp_class_application_handler - Implements CryptModuleSpecs::application_handler()
 */
int pgp_class_application_handler(struct Body *m, struct State *s)
{
  bool could_not_decrypt = false;
  int decrypt_okay_rc = 0;
  int needpass = -1;
  bool pgp_keyblock = false;
  bool clearsign = false;
  int rc = -1;
  int c = 1;
  long bytes;
  LOFF_T last_pos, offset;
  char buf[8192];
  FILE *fp_pgp_out = NULL, *fp_pgp_in = NULL, *fp_pgp_err = NULL;
  FILE *fp_tmp = NULL;
  pid_t pid;
  struct Buffer *tmpfname = mutt_buffer_pool_get();

  bool maybe_goodsig = true;
  bool have_any_sigs = false;

  char *gpgcharset = NULL;
  char body_charset[256];
  mutt_body_get_charset(m, body_charset, sizeof(body_charset));

  fseeko(s->fp_in, m->offset, SEEK_SET);
  last_pos = m->offset;

  for (bytes = m->length; bytes > 0;)
  {
    if (!fgets(buf, sizeof(buf), s->fp_in))
      break;

    offset = ftello(s->fp_in);
    bytes -= (offset - last_pos); /* don't rely on mutt_str_len(buf) */
    last_pos = offset;

    size_t plen = mutt_str_startswith(buf, "-----BEGIN PGP ");
    if (plen != 0)
    {
      clearsign = false;
      could_not_decrypt = false;
      decrypt_okay_rc = 0;

      if (mutt_str_startswith(buf + plen, "MESSAGE-----\n"))
        needpass = 1;
      else if (mutt_str_startswith(buf + plen, "SIGNED MESSAGE-----\n"))
      {
        clearsign = true;
        needpass = 0;
      }
      else if (mutt_str_startswith(buf + plen, "PUBLIC KEY BLOCK-----\n"))
      {
        needpass = 0;
        pgp_keyblock = true;
      }
      else
      {
        /* XXX we may wish to recode here */
        if (s->prefix)
          state_puts(s, s->prefix);
        state_puts(s, buf);
        continue;
      }

      have_any_sigs = have_any_sigs || (clearsign && (s->flags & MUTT_VERIFY));

      /* Copy PGP material to temporary file */
      mutt_buffer_mktemp(tmpfname);
      fp_tmp = mutt_file_fopen(mutt_b2s(tmpfname), "w+");
      if (!fp_tmp)
      {
        mutt_perror(mutt_b2s(tmpfname));
        FREE(&gpgcharset);
        goto out;
      }

      fputs(buf, fp_tmp);
      while ((bytes > 0) && fgets(buf, sizeof(buf) - 1, s->fp_in))
      {
        offset = ftello(s->fp_in);
        bytes -= (offset - last_pos); /* don't rely on mutt_str_len(buf) */
        last_pos = offset;

        fputs(buf, fp_tmp);

        if ((needpass && mutt_str_equal("-----END PGP MESSAGE-----\n", buf)) ||
            (!needpass &&
             (mutt_str_equal("-----END PGP SIGNATURE-----\n", buf) ||
              mutt_str_equal("-----END PGP PUBLIC KEY BLOCK-----\n", buf))))
        {
          break;
        }
        /* remember optional Charset: armor header as defined by RFC4880 */
        if (mutt_str_startswith(buf, "Charset: "))
        {
          size_t l = 0;
          FREE(&gpgcharset);
          gpgcharset = mutt_str_dup(buf + 9);
          l = mutt_str_len(gpgcharset);
          if ((l > 0) && (gpgcharset[l - 1] == '\n'))
            gpgcharset[l - 1] = 0;
          if (!mutt_ch_check_charset(gpgcharset, false))
            mutt_str_replace(&gpgcharset, "UTF-8");
        }
      }

      /* leave fp_tmp open in case we still need it - but flush it! */
      fflush(fp_tmp);

      /* Invoke PGP if needed */
      if (!clearsign || (s->flags & MUTT_VERIFY))
      {
        fp_pgp_out = mutt_file_mkstemp();
        if (!fp_pgp_out)
        {
          mutt_perror(_("Can't create temporary file"));
          goto out;
        }

        fp_pgp_err = mutt_file_mkstemp();
        if (!fp_pgp_err)
        {
          mutt_perror(_("Can't create temporary file"));
          goto out;
        }

        pid = pgp_invoke_decode(&fp_pgp_in, NULL, NULL, -1, fileno(fp_pgp_out),
                                fileno(fp_pgp_err), mutt_b2s(tmpfname), (needpass != 0));
        if (pid == -1)
        {
          mutt_file_fclose(&fp_pgp_out);
          maybe_goodsig = false;
          fp_pgp_in = NULL;
          state_attach_puts(
              s, _("[-- Error: unable to create PGP subprocess --]\n"));
        }
        else /* PGP started successfully */
        {
          if (needpass)
          {
            if (!pgp_class_valid_passphrase())
              pgp_class_void_passphrase();
            if (pgp_use_gpg_agent())
              *PgpPass = '\0';
            fprintf(fp_pgp_in, "%s\n", PgpPass);
          }

          mutt_file_fclose(&fp_pgp_in);

          int wait_filter_rc = filter_wait(pid);

          fflush(fp_pgp_err);
          /* If we are expecting an encrypted message, verify status fd output.
           * Note that BEGIN PGP MESSAGE does not guarantee the content is encrypted,
           * so we need to be more selective about the value of decrypt_okay_rc.
           *
           * -3 indicates we actively found a DECRYPTION_FAILED.
           * -2 and -1 indicate part or all of the content was plaintext.  */
          if (needpass)
          {
            rewind(fp_pgp_err);
            decrypt_okay_rc = pgp_check_decryption_okay(fp_pgp_err);
            if (decrypt_okay_rc <= -3)
              mutt_file_fclose(&fp_pgp_out);
          }

          if (s->flags & MUTT_DISPLAY)
          {
            rewind(fp_pgp_err);
            crypt_current_time(s, "PGP");
            int checksig_rc = pgp_copy_checksig(fp_pgp_err, s->fp_out);

            if (checksig_rc == 0)
              have_any_sigs = true;
            /* Sig is bad if
             * gpg_good_sign-pattern did not match || pgp_decode_command returned not 0
             * Sig _is_ correct if
             *  gpg_good_sign="" && pgp_decode_command returned 0 */
            if (checksig_rc == -1 || (wait_filter_rc != 0))
              maybe_goodsig = false;

            state_attach_puts(s, _("[-- End of PGP output --]\n\n"));
          }
          if (pgp_use_gpg_agent())
            mutt_need_hard_redraw();
        }

        /* treat empty result as sign of failure */
        /* TODO: maybe on failure neomutt should include the original undecoded text. */
        if (fp_pgp_out)
        {
          rewind(fp_pgp_out);
          c = fgetc(fp_pgp_out);
          ungetc(c, fp_pgp_out);
        }
        if (!clearsign && (!fp_pgp_out || (c == EOF)))
        {
          could_not_decrypt = true;
          pgp_class_void_passphrase();
        }

        if ((could_not_decrypt || (decrypt_okay_rc <= -3)) && !(s->flags & MUTT_DISPLAY))
        {
          mutt_error(_("Could not decrypt PGP message"));
          goto out;
        }
      }

      /* Now, copy cleartext to the screen.  */
      if (s->flags & MUTT_DISPLAY)
      {
        if (needpass)
          state_attach_puts(s, _("[-- BEGIN PGP MESSAGE --]\n\n"));
        else if (pgp_keyblock)
          state_attach_puts(s, _("[-- BEGIN PGP PUBLIC KEY BLOCK --]\n"));
        else
          state_attach_puts(s, _("[-- BEGIN PGP SIGNED MESSAGE --]\n\n"));
      }

      if (clearsign)
      {
        rewind(fp_tmp);
        pgp_copy_clearsigned(fp_tmp, s, body_charset);
      }
      else if (fp_pgp_out)
      {
        struct FgetConv *fc = NULL;
        int ch;
        char *expected_charset = (gpgcharset && *gpgcharset) ? gpgcharset : "utf-8";

        mutt_debug(LL_DEBUG3, "pgp: recoding inline from [%s] to [%s]\n",
                   expected_charset, C_Charset);

        rewind(fp_pgp_out);
        state_set_prefix(s);
        fc = mutt_ch_fgetconv_open(fp_pgp_out, expected_charset, C_Charset, MUTT_ICONV_HOOK_FROM);
        while ((ch = mutt_ch_fgetconv(fc)) != EOF)
          state_prefix_putc(s, ch);
        mutt_ch_fgetconv_close(&fc);
      }

      /* Multiple PGP blocks can exist, so these need to be closed and
       * unlinked inside the loop.  */
      mutt_file_fclose(&fp_tmp);
      mutt_file_unlink(mutt_b2s(tmpfname));
      mutt_file_fclose(&fp_pgp_out);
      mutt_file_fclose(&fp_pgp_err);

      if (s->flags & MUTT_DISPLAY)
      {
        state_putc(s, '\n');
        if (needpass)
        {
          state_attach_puts(s, _("[-- END PGP MESSAGE --]\n"));
          if (could_not_decrypt || (decrypt_okay_rc <= -3))
            mutt_error(_("Could not decrypt PGP message"));
          else if (decrypt_okay_rc < 0)
          {
            /* L10N: You will see this error message if (1) you are decrypting
               (not encrypting) something and (2) it is a plaintext. So the
               message does not mean "You failed to encrypt the message." */
            mutt_error(_("PGP message is not encrypted"));
          }
          else
            mutt_message(_("PGP message successfully decrypted"));
        }
        else if (pgp_keyblock)
          state_attach_puts(s, _("[-- END PGP PUBLIC KEY BLOCK --]\n"));
        else
          state_attach_puts(s, _("[-- END PGP SIGNED MESSAGE --]\n"));
      }
    }
    else
    {
      /* A traditional PGP part may mix signed and unsigned content */
      /* XXX we may wish to recode here */
      if (s->prefix)
        state_puts(s, s->prefix);
      state_puts(s, buf);
    }
  }

  rc = 0;

out:
  m->goodsig = (maybe_goodsig && have_any_sigs);

  if (fp_tmp)
  {
    mutt_file_fclose(&fp_tmp);
    mutt_file_unlink(mutt_b2s(tmpfname));
  }
  mutt_file_fclose(&fp_pgp_out);
  mutt_file_fclose(&fp_pgp_err);

  mutt_buffer_pool_release(&tmpfname);

  FREE(&gpgcharset);

  if (needpass == -1)
  {
    state_attach_puts(
        s, _("[-- Error: could not find beginning of PGP message --]\n\n"));
    return -1;
  }

  return rc;
}

/**
 * pgp_check_traditional_one_body - Check the body of an inline PGP message
 * @param fp File to read
 * @param b  Body to populate
 * @retval 1 Success
 * @retval 0 Error
 */
static int pgp_check_traditional_one_body(FILE *fp, struct Body *b)
{
  struct Buffer *tempfile = NULL;
  char buf[8192];
  int rc = 0;

  bool sgn = false;
  bool enc = false;
  bool key = false;

  if (b->type != TYPE_TEXT)
    goto cleanup;

  tempfile = mutt_buffer_pool_get();
  mutt_buffer_mktemp(tempfile);
  if (mutt_decode_save_attachment(fp, b, mutt_b2s(tempfile), 0, MUTT_SAVE_NO_FLAGS) != 0)
  {
    unlink(mutt_b2s(tempfile));
    goto cleanup;
  }

  FILE *fp_tmp = fopen(mutt_b2s(tempfile), "r");
  if (!fp_tmp)
  {
    unlink(mutt_b2s(tempfile));
    goto cleanup;
  }

  while (fgets(buf, sizeof(buf), fp_tmp))
  {
    size_t plen = mutt_str_startswith(buf, "-----BEGIN PGP ");
    if (plen != 0)
    {
      if (mutt_str_startswith(buf + plen, "MESSAGE-----\n"))
        enc = true;
      else if (mutt_str_startswith(buf + plen, "SIGNED MESSAGE-----\n"))
        sgn = true;
      else if (mutt_str_startswith(buf + plen, "PUBLIC KEY BLOCK-----\n"))
        key = true;
    }
  }
  mutt_file_fclose(&fp_tmp);
  unlink(mutt_b2s(tempfile));

  if (!enc && !sgn && !key)
    goto cleanup;

  /* fix the content type */

  mutt_param_set(&b->parameter, "format", "fixed");
  if (enc)
    mutt_param_set(&b->parameter, "x-action", "pgp-encrypted");
  else if (sgn)
    mutt_param_set(&b->parameter, "x-action", "pgp-signed");
  else if (key)
    mutt_param_set(&b->parameter, "x-action", "pgp-keys");

  rc = 1;

cleanup:
  mutt_buffer_pool_release(&tempfile);
  return rc;
}

/**
 * pgp_class_check_traditional - Implements CryptModuleSpecs::pgp_check_traditional()
 */
int pgp_class_check_traditional(FILE *fp, struct Body *b, bool just_one)
{
  int rc = 0;
  int r;
  for (; b; b = b->next)
  {
    if (!just_one && is_multipart(b))
      rc = pgp_class_check_traditional(fp, b->parts, false) || rc;
    else if (b->type == TYPE_TEXT)
    {
      r = mutt_is_application_pgp(b);
      if (r)
        rc = rc || r;
      else
        rc = pgp_check_traditional_one_body(fp, b) || rc;
    }

    if (just_one)
      break;
  }

  return rc;
}

/**
 * pgp_class_verify_one - Implements CryptModuleSpecs::verify_one()
 */
int pgp_class_verify_one(struct Body *sigbdy, struct State *s, const char *tempfile)
{
  FILE *fp_pgp_out = NULL;
  pid_t pid;
  int badsig = -1;
  struct Buffer *sigfile = mutt_buffer_pool_get();

  mutt_buffer_printf(sigfile, "%s.asc", tempfile);

  FILE *fp_sig = mutt_file_fopen(mutt_b2s(sigfile), "w");
  if (!fp_sig)
  {
    mutt_perror(mutt_b2s(sigfile));
    goto cleanup;
  }

  fseeko(s->fp_in, sigbdy->offset, SEEK_SET);
  mutt_file_copy_bytes(s->fp_in, fp_sig, sigbdy->length);
  mutt_file_fclose(&fp_sig);

  FILE *fp_pgp_err = mutt_file_mkstemp();
  if (!fp_pgp_err)
  {
    mutt_perror(_("Can't create temporary file"));
    unlink(mutt_b2s(sigfile));
    goto cleanup;
  }

  crypt_current_time(s, "PGP");

  pid = pgp_invoke_verify(NULL, &fp_pgp_out, NULL, -1, -1, fileno(fp_pgp_err),
                          tempfile, mutt_b2s(sigfile));
  if (pid != -1)
  {
    if (pgp_copy_checksig(fp_pgp_out, s->fp_out) >= 0)
      badsig = 0;

    mutt_file_fclose(&fp_pgp_out);
    fflush(fp_pgp_err);
    rewind(fp_pgp_err);

    if (pgp_copy_checksig(fp_pgp_err, s->fp_out) >= 0)
      badsig = 0;

    const int rv = filter_wait(pid);
    if (rv)
      badsig = -1;

    mutt_debug(LL_DEBUG1, "filter_wait returned %d\n", rv);
  }

  mutt_file_fclose(&fp_pgp_err);

  state_attach_puts(s, _("[-- End of PGP output --]\n\n"));

  mutt_file_unlink(mutt_b2s(sigfile));

cleanup:
  mutt_buffer_pool_release(&sigfile);

  mutt_debug(LL_DEBUG1, "returning %d\n", badsig);
  return badsig;
}

/**
 * pgp_extract_keys_from_attachment - Extract pgp keys from messages/attachments
 * @param fp  File to read from
 * @param top Top Attachment
 */
static void pgp_extract_keys_from_attachment(FILE *fp, struct Body *top)
{
  struct State s = { 0 };
  struct Buffer *tempfname = mutt_buffer_pool_get();

  mutt_buffer_mktemp(tempfname);
  FILE *fp_tmp = mutt_file_fopen(mutt_b2s(tempfname), "w");
  if (!fp_tmp)
  {
    mutt_perror(mutt_b2s(tempfname));
    goto cleanup;
  }

  s.fp_in = fp;
  s.fp_out = fp_tmp;

  mutt_body_handler(top, &s);

  mutt_file_fclose(&fp_tmp);

  pgp_class_invoke_import(mutt_b2s(tempfname));
  mutt_any_key_to_continue(NULL);

  mutt_file_unlink(mutt_b2s(tempfname));

cleanup:
  mutt_buffer_pool_release(&tempfname);
}

/**
 * pgp_class_extract_key_from_attachment - Implements CryptModuleSpecs::pgp_extract_key_from_attachment()
 */
void pgp_class_extract_key_from_attachment(FILE *fp, struct Body *top)
{
  if (!fp)
  {
    mutt_error(_("Internal error.  Please submit a bug report."));
    return;
  }

  mutt_endwin();

  OptDontHandlePgpKeys = true;
  pgp_extract_keys_from_attachment(fp, top);
  OptDontHandlePgpKeys = false;
}

/**
 * pgp_decrypt_part - Decrypt part of a PGP message
 * @param a      Body of attachment
 * @param s      State to use
 * @param fp_out File to write to
 * @param p      Body of parent (main email)
 * @retval ptr New Body for the attachment
 */
static struct Body *pgp_decrypt_part(struct Body *a, struct State *s,
                                     FILE *fp_out, struct Body *p)
{
  if (!a || !s || !fp_out || !p)
    return NULL;

  char buf[1024];
  FILE *fp_pgp_in = NULL, *fp_pgp_out = NULL, *fp_pgp_tmp = NULL;
  struct stat info;
  struct Body *tattach = NULL;
  pid_t pid;
  int rv;
  struct Buffer *pgptmpfile = mutt_buffer_pool_get();

  FILE *fp_pgp_err = mutt_file_mkstemp();
  if (!fp_pgp_err)
  {
    mutt_perror(_("Can't create temporary file"));
    goto cleanup;
  }

  mutt_buffer_mktemp(pgptmpfile);
  fp_pgp_tmp = mutt_file_fopen(mutt_b2s(pgptmpfile), "w");
  if (!fp_pgp_tmp)
  {
    mutt_perror(mutt_b2s(pgptmpfile));
    mutt_file_fclose(&fp_pgp_err);
    goto cleanup;
  }

  /* Position the stream at the beginning of the body, and send the data to
   * the temporary file.  */

  fseeko(s->fp_in, a->offset, SEEK_SET);
  mutt_file_copy_bytes(s->fp_in, fp_pgp_tmp, a->length);
  mutt_file_fclose(&fp_pgp_tmp);

  pid = pgp_invoke_decrypt(&fp_pgp_in, &fp_pgp_out, NULL, -1, -1,
                           fileno(fp_pgp_err), mutt_b2s(pgptmpfile));
  if (pid == -1)
  {
    mutt_file_fclose(&fp_pgp_err);
    unlink(mutt_b2s(pgptmpfile));
    if (s->flags & MUTT_DISPLAY)
    {
      state_attach_puts(
          s, _("[-- Error: could not create a PGP subprocess --]\n\n"));
    }
    goto cleanup;
  }

  /* send the PGP passphrase to the subprocess.  Never do this if the agent is
   * active, because this might lead to a passphrase send as the message. */
  if (!pgp_use_gpg_agent())
    fputs(PgpPass, fp_pgp_in);
  fputc('\n', fp_pgp_in);
  mutt_file_fclose(&fp_pgp_in);

  /* Read the output from PGP, and make sure to change CRLF to LF, otherwise
   * read_mime_header has a hard time parsing the message.  */
  while (fgets(buf, sizeof(buf) - 1, fp_pgp_out))
  {
    size_t len = mutt_str_len(buf);
    if ((len > 1) && (buf[len - 2] == '\r'))
      strcpy(buf + len - 2, "\n");
    fputs(buf, fp_out);
  }

  mutt_file_fclose(&fp_pgp_out);
  rv = filter_wait(pid);
  mutt_file_unlink(mutt_b2s(pgptmpfile));

  fflush(fp_pgp_err);
  rewind(fp_pgp_err);
  if (pgp_check_decryption_okay(fp_pgp_err) < 0)
  {
    mutt_error(_("Decryption failed"));
    pgp_class_void_passphrase();
    mutt_file_fclose(&fp_pgp_err);
    goto cleanup;
  }

  if (s->flags & MUTT_DISPLAY)
  {
    rewind(fp_pgp_err);
    if ((pgp_copy_checksig(fp_pgp_err, s->fp_out) == 0) && !rv)
      p->goodsig = true;
    else
      p->goodsig = false;
    state_attach_puts(s, _("[-- End of PGP output --]\n\n"));
  }
  mutt_file_fclose(&fp_pgp_err);

  fflush(fp_out);
  rewind(fp_out);

  if (pgp_use_gpg_agent())
    mutt_need_hard_redraw();

  if (fgetc(fp_out) == EOF)
  {
    mutt_error(_("Decryption failed"));
    pgp_class_void_passphrase();
    goto cleanup;
  }

  rewind(fp_out);

  tattach = mutt_read_mime_header(fp_out, 0);
  if (tattach)
  {
    /* Need to set the length of this body part.  */
    fstat(fileno(fp_out), &info);
    tattach->length = info.st_size - tattach->offset;

    /* See if we need to recurse on this MIME part.  */
    mutt_parse_part(fp_out, tattach);
  }

cleanup:
  mutt_buffer_pool_release(&pgptmpfile);
  return tattach;
}

/**
 * pgp_class_decrypt_mime - Implements CryptModuleSpecs::decrypt_mime()
 */
int pgp_class_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **cur)
{
  struct State s = { 0 };
  struct Body *p = b;
  bool need_decode = false;
  int saved_type = 0;
  LOFF_T saved_offset = 0;
  size_t saved_length = 0;
  FILE *fp_decoded = NULL;
  int rc = 0;

  if (mutt_is_valid_multipart_pgp_encrypted(b))
  {
    b = b->parts->next;
    /* Some clients improperly encode the octetstream part. */
    if (b->encoding != ENC_7BIT)
      need_decode = true;
  }
  else if (mutt_is_malformed_multipart_pgp_encrypted(b))
  {
    b = b->parts->next->next;
    need_decode = true;
  }
  else
    return -1;

  s.fp_in = fp_in;

  if (need_decode)
  {
    saved_type = b->type;
    saved_offset = b->offset;
    saved_length = b->length;

    fp_decoded = mutt_file_mkstemp();
    if (!fp_decoded)
    {
      mutt_perror(_("Can't create temporary file"));
      return -1;
    }

    fseeko(s.fp_in, b->offset, SEEK_SET);
    s.fp_out = fp_decoded;

    mutt_decode_attachment(b, &s);

    fflush(fp_decoded);
    b->length = ftello(fp_decoded);
    b->offset = 0;
    rewind(fp_decoded);
    s.fp_in = fp_decoded;
    s.fp_out = 0;
  }

  *fp_out = mutt_file_mkstemp();
  if (!*fp_out)
  {
    mutt_perror(_("Can't create temporary file"));
    rc = -1;
    goto bail;
  }

  *cur = pgp_decrypt_part(b, &s, *fp_out, p);
  if (!*cur)
    rc = -1;
  rewind(*fp_out);

bail:
  if (need_decode)
  {
    b->type = saved_type;
    b->length = saved_length;
    b->offset = saved_offset;
    mutt_file_fclose(&fp_decoded);
  }

  return rc;
}

/**
 * pgp_class_encrypted_handler - Implements CryptModuleSpecs::encrypted_handler()
 */
int pgp_class_encrypted_handler(struct Body *a, struct State *s)
{
  FILE *fp_in = NULL;
  struct Body *tattach = NULL;
  int rc = 0;

  FILE *fp_out = mutt_file_mkstemp();
  if (!fp_out)
  {
    mutt_perror(_("Can't create temporary file"));
    if (s->flags & MUTT_DISPLAY)
    {
      state_attach_puts(s,
                        _("[-- Error: could not create temporary file --]\n"));
    }
    return -1;
  }

  if (s->flags & MUTT_DISPLAY)
    crypt_current_time(s, "PGP");

  tattach = pgp_decrypt_part(a, s, fp_out, a);
  if (tattach)
  {
    if (s->flags & MUTT_DISPLAY)
    {
      state_attach_puts(
          s, _("[-- The following data is PGP/MIME encrypted --]\n\n"));
      mutt_protected_headers_handler(tattach, s);
    }

    /* Store any protected headers in the parent so they can be
     * accessed for index updates after the handler recursion is done.
     * This is done before the handler to prevent a nested encrypted
     * handler from freeing the headers. */
    mutt_env_free(&a->mime_headers);
    a->mime_headers = tattach->mime_headers;
    tattach->mime_headers = NULL;

    fp_in = s->fp_in;
    s->fp_in = fp_out;
    rc = mutt_body_handler(tattach, s);
    s->fp_in = fp_in;

    /* Embedded multipart signed protected headers override the
     * encrypted headers.  We need to do this after the handler so
     * they can be printed in the pager. */
    if (mutt_is_multipart_signed(tattach) && tattach->parts && tattach->parts->mime_headers)
    {
      mutt_env_free(&a->mime_headers);
      a->mime_headers = tattach->parts->mime_headers;
      tattach->parts->mime_headers = NULL;
    }

    /* if a multipart/signed is the _only_ sub-part of a
     * multipart/encrypted, cache signature verification
     * status.  */
    if (mutt_is_multipart_signed(tattach) && !tattach->next)
      a->goodsig |= tattach->goodsig;

    if (s->flags & MUTT_DISPLAY)
    {
      state_puts(s, "\n");
      state_attach_puts(s, _("[-- End of PGP/MIME encrypted data --]\n"));
    }

    mutt_body_free(&tattach);
    /* clear 'Invoking...' message, since there's no error */
    mutt_message(_("PGP message successfully decrypted"));
  }
  else
  {
    mutt_error(_("Could not decrypt PGP message"));
    /* void the passphrase, even if it's not necessarily the problem */
    pgp_class_void_passphrase();
    rc = -1;
  }

  mutt_file_fclose(&fp_out);

  return rc;
}

/* ----------------------------------------------------------------------------
 * Routines for sending PGP/MIME messages.
 */

/**
 * pgp_class_sign_message - Implements CryptModuleSpecs::sign_message()
 */
struct Body *pgp_class_sign_message(struct Body *a, const struct AddressList *from)
{
  struct Body *t = NULL, *rv = NULL;
  char buf[1024];
  FILE *fp_pgp_in = NULL, *fp_pgp_out = NULL, *fp_pgp_err = NULL, *fp_signed = NULL;
  bool err = false;
  bool empty = true;
  pid_t pid;
  struct Buffer *sigfile = mutt_buffer_pool_get();
  struct Buffer *signedfile = mutt_buffer_pool_get();

  crypt_convert_to_7bit(a); /* Signed data _must_ be in 7-bit format. */

  mutt_buffer_mktemp(sigfile);
  FILE *fp_sig = mutt_file_fopen(mutt_b2s(sigfile), "w");
  if (!fp_sig)
  {
    goto cleanup;
  }

  mutt_buffer_mktemp(signedfile);
  fp_signed = mutt_file_fopen(mutt_b2s(signedfile), "w");
  if (!fp_signed)
  {
    mutt_perror(mutt_b2s(signedfile));
    mutt_file_fclose(&fp_sig);
    unlink(mutt_b2s(sigfile));
    goto cleanup;
  }

  mutt_write_mime_header(a, fp_signed, NeoMutt->sub);
  fputc('\n', fp_signed);
  mutt_write_mime_body(a, fp_signed, NeoMutt->sub);
  mutt_file_fclose(&fp_signed);

  pid = pgp_invoke_sign(&fp_pgp_in, &fp_pgp_out, &fp_pgp_err, -1, -1, -1,
                        mutt_b2s(signedfile));
  if (pid == -1)
  {
    mutt_perror(_("Can't open PGP subprocess"));
    mutt_file_fclose(&fp_sig);
    unlink(mutt_b2s(sigfile));
    unlink(mutt_b2s(signedfile));
    goto cleanup;
  }

  if (!pgp_use_gpg_agent())
    fputs(PgpPass, fp_pgp_in);
  fputc('\n', fp_pgp_in);
  mutt_file_fclose(&fp_pgp_in);

  /* Read back the PGP signature.  Also, change MESSAGE=>SIGNATURE as
   * recommended for future releases of PGP.  */
  while (fgets(buf, sizeof(buf) - 1, fp_pgp_out))
  {
    if (mutt_str_equal("-----BEGIN PGP MESSAGE-----\n", buf))
      fputs("-----BEGIN PGP SIGNATURE-----\n", fp_sig);
    else if (mutt_str_equal("-----END PGP MESSAGE-----\n", buf))
      fputs("-----END PGP SIGNATURE-----\n", fp_sig);
    else
      fputs(buf, fp_sig);
    empty = false; /* got some output, so we're ok */
  }

  /* check for errors from PGP */
  err = false;
  while (fgets(buf, sizeof(buf) - 1, fp_pgp_err))
  {
    err = true;
    fputs(buf, stdout);
  }

  if (filter_wait(pid) && C_PgpCheckExit)
    empty = true;

  mutt_file_fclose(&fp_pgp_err);
  mutt_file_fclose(&fp_pgp_out);
  unlink(mutt_b2s(signedfile));

  if (mutt_file_fclose(&fp_sig) != 0)
  {
    mutt_perror("fclose");
    unlink(mutt_b2s(sigfile));
    goto cleanup;
  }

  if (err)
    mutt_any_key_to_continue(NULL);
  if (empty)
  {
    unlink(mutt_b2s(sigfile));
    /* most likely error is a bad passphrase, so automatically forget it */
    pgp_class_void_passphrase();
    goto cleanup; /* fatal error while signing */
  }

  t = mutt_body_new();
  t->type = TYPE_MULTIPART;
  t->subtype = mutt_str_dup("signed");
  t->encoding = ENC_7BIT;
  t->use_disp = false;
  t->disposition = DISP_INLINE;
  rv = t;

  mutt_generate_boundary(&t->parameter);
  mutt_param_set(&t->parameter, "protocol", "application/pgp-signature");
  mutt_param_set(&t->parameter, "micalg", pgp_micalg(mutt_b2s(sigfile)));

  t->parts = a;

  t->parts->next = mutt_body_new();
  t = t->parts->next;
  t->type = TYPE_APPLICATION;
  t->subtype = mutt_str_dup("pgp-signature");
  t->filename = mutt_buffer_strdup(sigfile);
  t->use_disp = false;
  t->disposition = DISP_NONE;
  t->encoding = ENC_7BIT;
  t->unlink = true; /* ok to remove this file after sending. */
  mutt_param_set(&t->parameter, "name", "signature.asc");

cleanup:
  mutt_buffer_pool_release(&sigfile);
  mutt_buffer_pool_release(&signedfile);
  return rv;
}

/**
 * pgp_class_find_keys - Implements CryptModuleSpecs::find_keys()
 */
char *pgp_class_find_keys(struct AddressList *addrlist, bool oppenc_mode)
{
  struct ListHead crypt_hook_list = STAILQ_HEAD_INITIALIZER(crypt_hook_list);
  struct ListNode *crypt_hook = NULL;
  const char *keyid = NULL;
  char *keylist = NULL;
  size_t keylist_size = 0;
  size_t keylist_used = 0;
  struct Address *p = NULL;
  struct PgpKeyInfo *k_info = NULL;
  const char *fqdn = mutt_fqdn(true, NeoMutt->sub);
  char buf[1024];
  bool key_selected;
  struct AddressList hookal = TAILQ_HEAD_INITIALIZER(hookal);

  struct Address *a = NULL;
  TAILQ_FOREACH(a, addrlist, entries)
  {
    key_selected = false;
    mutt_crypt_hook(&crypt_hook_list, a);
    crypt_hook = STAILQ_FIRST(&crypt_hook_list);
    do
    {
      p = a;
      k_info = NULL;

      if (crypt_hook)
      {
        keyid = crypt_hook->data;
        enum QuadOption ans = MUTT_YES;
        if (!oppenc_mode && C_CryptConfirmhook)
        {
          snprintf(buf, sizeof(buf), _("Use keyID = \"%s\" for %s?"), keyid, p->mailbox);
          ans = mutt_yesorno(buf, MUTT_YES);
        }
        if (ans == MUTT_YES)
        {
          if (crypt_is_numerical_keyid(keyid))
          {
            if (mutt_strn_equal(keyid, "0x", 2))
              keyid += 2;
            goto bypass_selection; /* you don't see this. */
          }

          /* check for e-mail address */
          mutt_addrlist_clear(&hookal);
          if (strchr(keyid, '@') && (mutt_addrlist_parse(&hookal, keyid) != 0))
          {
            mutt_addrlist_qualify(&hookal, fqdn);
            p = TAILQ_FIRST(&hookal);
          }
          else if (!oppenc_mode)
          {
            k_info = pgp_getkeybystr(keyid, KEYFLAG_CANENCRYPT, PGP_PUBRING);
          }
        }
        else if (ans == MUTT_NO)
        {
          if (key_selected || STAILQ_NEXT(crypt_hook, entries))
          {
            crypt_hook = STAILQ_NEXT(crypt_hook, entries);
            continue;
          }
        }
        else if (ans == MUTT_ABORT)
        {
          FREE(&keylist);
          mutt_addrlist_clear(&hookal);
          mutt_list_free(&crypt_hook_list);
          return NULL;
        }
      }

      if (!k_info)
      {
        pgp_class_invoke_getkeys(p);
        k_info = pgp_getkeybyaddr(p, KEYFLAG_CANENCRYPT, PGP_PUBRING, oppenc_mode);
      }

      if (!k_info && !oppenc_mode)
      {
        snprintf(buf, sizeof(buf), _("Enter keyID for %s: "), p->mailbox);
        k_info = pgp_ask_for_key(buf, p->mailbox, KEYFLAG_CANENCRYPT, PGP_PUBRING);
      }

      if (!k_info)
      {
        FREE(&keylist);
        mutt_addrlist_clear(&hookal);
        mutt_list_free(&crypt_hook_list);
        return NULL;
      }

      keyid = pgp_fpr_or_lkeyid(k_info);

    bypass_selection:
      keylist_size += mutt_str_len(keyid) + 4;
      mutt_mem_realloc(&keylist, keylist_size);
      sprintf(keylist + keylist_used, "%s0x%s", keylist_used ? " " : "", keyid);
      keylist_used = mutt_str_len(keylist);

      key_selected = true;

      pgp_key_free(&k_info);
      mutt_addrlist_clear(&hookal);

      if (crypt_hook)
        crypt_hook = STAILQ_NEXT(crypt_hook, entries);

    } while (crypt_hook);

    mutt_list_free(&crypt_hook_list);
  }
  return keylist;
}

/**
 * pgp_class_encrypt_message - Implements CryptModuleSpecs::pgp_encrypt_message()
 *
 * @warning "a" is no longer freed in this routine, you need to free it later.
 * This is necessary for $fcc_attach.
 */
struct Body *pgp_class_encrypt_message(struct Body *a, char *keylist, bool sign,
                                       const struct AddressList *from)
{
  char buf[1024];
  FILE *fp_pgp_in = NULL, *fp_tmp = NULL;
  struct Body *t = NULL;
  int err = 0;
  bool empty = false;
  pid_t pid;
  struct Buffer *tempfile = mutt_buffer_pool_get();
  struct Buffer *pgpinfile = mutt_buffer_pool_get();

  mutt_buffer_mktemp(tempfile);
  FILE *fp_out = mutt_file_fopen(mutt_b2s(tempfile), "w+");
  if (!fp_out)
  {
    mutt_perror(mutt_b2s(tempfile));
    goto cleanup;
  }

  FILE *fp_pgp_err = mutt_file_mkstemp();
  if (!fp_pgp_err)
  {
    mutt_perror(_("Can't create temporary file"));
    unlink(mutt_b2s(tempfile));
    mutt_file_fclose(&fp_out);
    goto cleanup;
  }

  mutt_buffer_mktemp(pgpinfile);
  fp_tmp = mutt_file_fopen(mutt_b2s(pgpinfile), "w");
  if (!fp_tmp)
  {
    mutt_perror(mutt_b2s(pgpinfile));
    unlink(mutt_b2s(tempfile));
    mutt_file_fclose(&fp_out);
    mutt_file_fclose(&fp_pgp_err);
    goto cleanup;
  }

  if (sign)
    crypt_convert_to_7bit(a);

  mutt_write_mime_header(a, fp_tmp, NeoMutt->sub);
  fputc('\n', fp_tmp);
  mutt_write_mime_body(a, fp_tmp, NeoMutt->sub);
  mutt_file_fclose(&fp_tmp);

  pid = pgp_invoke_encrypt(&fp_pgp_in, NULL, NULL, -1, fileno(fp_out),
                           fileno(fp_pgp_err), mutt_b2s(pgpinfile), keylist, sign);
  if (pid == -1)
  {
    mutt_file_fclose(&fp_out);
    mutt_file_fclose(&fp_pgp_err);
    unlink(mutt_b2s(pgpinfile));
    goto cleanup;
  }

  if (sign)
  {
    if (!pgp_use_gpg_agent())
      fputs(PgpPass, fp_pgp_in);
    fputc('\n', fp_pgp_in);
  }
  mutt_file_fclose(&fp_pgp_in);

  if (filter_wait(pid) && C_PgpCheckExit)
    empty = true;

  unlink(mutt_b2s(pgpinfile));

  fflush(fp_out);
  rewind(fp_out);
  if (!empty)
    empty = (fgetc(fp_out) == EOF);
  mutt_file_fclose(&fp_out);

  fflush(fp_pgp_err);
  rewind(fp_pgp_err);
  while (fgets(buf, sizeof(buf) - 1, fp_pgp_err))
  {
    err = 1;
    fputs(buf, stdout);
  }
  mutt_file_fclose(&fp_pgp_err);

  /* pause if there is any error output from PGP */
  if (err)
    mutt_any_key_to_continue(NULL);

  if (empty)
  {
    /* fatal error while trying to encrypt message */
    if (sign)
      pgp_class_void_passphrase(); /* just in case */
    unlink(mutt_b2s(tempfile));
    goto cleanup;
  }

  t = mutt_body_new();
  t->type = TYPE_MULTIPART;
  t->subtype = mutt_str_dup("encrypted");
  t->encoding = ENC_7BIT;
  t->use_disp = false;
  t->disposition = DISP_INLINE;

  mutt_generate_boundary(&t->parameter);
  mutt_param_set(&t->parameter, "protocol", "application/pgp-encrypted");

  t->parts = mutt_body_new();
  t->parts->type = TYPE_APPLICATION;
  t->parts->subtype = mutt_str_dup("pgp-encrypted");
  t->parts->encoding = ENC_7BIT;

  t->parts->next = mutt_body_new();
  t->parts->next->type = TYPE_APPLICATION;
  t->parts->next->subtype = mutt_str_dup("octet-stream");
  t->parts->next->encoding = ENC_7BIT;
  t->parts->next->filename = mutt_buffer_strdup(tempfile);
  t->parts->next->use_disp = true;
  t->parts->next->disposition = DISP_ATTACH;
  t->parts->next->unlink = true; /* delete after sending the message */
  t->parts->next->d_filename = mutt_str_dup("msg.asc"); /* non pgp/mime can save */

cleanup:
  mutt_buffer_pool_release(&tempfile);
  mutt_buffer_pool_release(&pgpinfile);
  return t;
}

/**
 * pgp_class_traditional_encryptsign - Implements CryptModuleSpecs::pgp_traditional_encryptsign()
 */
struct Body *pgp_class_traditional_encryptsign(struct Body *a, SecurityFlags flags, char *keylist)
{
  struct Body *b = NULL;
  char body_charset[256];
  char *from_charset = NULL;
  const char *send_charset = NULL;
  bool empty = false;
  bool err;
  char buf[256];
  pid_t pid;
  struct Buffer *pgpinfile = mutt_buffer_pool_get();
  struct Buffer *pgpoutfile = mutt_buffer_pool_get();

  if (a->type != TYPE_TEXT)
    goto cleanup;
  if (!mutt_istr_equal(a->subtype, "plain"))
    goto cleanup;

  FILE *fp_body = fopen(a->filename, "r");
  if (!fp_body)
  {
    mutt_perror(a->filename);
    goto cleanup;
  }

  mutt_buffer_mktemp(pgpinfile);
  FILE *fp_pgp_in = mutt_file_fopen(mutt_b2s(pgpinfile), "w");
  if (!fp_pgp_in)
  {
    mutt_perror(mutt_b2s(pgpinfile));
    mutt_file_fclose(&fp_body);
    goto cleanup;
  }

  /* The following code is really correct:  If noconv is set,
   * a's charset parameter contains the on-disk character set, and
   * we have to convert from that to utf-8.  If noconv is not set,
   * we have to convert from $charset to utf-8.  */

  mutt_body_get_charset(a, body_charset, sizeof(body_charset));
  if (a->noconv)
    from_charset = body_charset;
  else
    from_charset = C_Charset;

  if (!mutt_ch_is_us_ascii(body_charset))
  {
    int c;
    struct FgetConv *fc = NULL;

    if (flags & SEC_ENCRYPT)
      send_charset = "us-ascii";
    else
      send_charset = "utf-8";

    /* fromcode is assumed to be correct: we set flags to 0 */
    fc = mutt_ch_fgetconv_open(fp_body, from_charset, "utf-8", 0);
    while ((c = mutt_ch_fgetconv(fc)) != EOF)
      fputc(c, fp_pgp_in);

    mutt_ch_fgetconv_close(&fc);
  }
  else
  {
    send_charset = "us-ascii";
    mutt_file_copy_stream(fp_body, fp_pgp_in);
  }
  mutt_file_fclose(&fp_body);
  mutt_file_fclose(&fp_pgp_in);

  mutt_buffer_mktemp(pgpoutfile);
  FILE *fp_pgp_out = mutt_file_fopen(mutt_b2s(pgpoutfile), "w+");
  FILE *fp_pgp_err = mutt_file_mkstemp();
  if (!fp_pgp_out || !fp_pgp_err)
  {
    mutt_perror(fp_pgp_out ? "Can't create temporary file" : mutt_b2s(pgpoutfile));
    unlink(mutt_b2s(pgpinfile));
    if (fp_pgp_out)
    {
      mutt_file_fclose(&fp_pgp_out);
      unlink(mutt_b2s(pgpoutfile));
    }
    mutt_file_fclose(&fp_pgp_err);
    goto cleanup;
  }

  pid = pgp_invoke_traditional(&fp_pgp_in, NULL, NULL, -1, fileno(fp_pgp_out),
                               fileno(fp_pgp_err), mutt_b2s(pgpinfile), keylist, flags);
  if (pid == -1)
  {
    mutt_perror(_("Can't invoke PGP"));
    mutt_file_fclose(&fp_pgp_out);
    mutt_file_fclose(&fp_pgp_err);
    mutt_file_unlink(mutt_b2s(pgpinfile));
    unlink(mutt_b2s(pgpoutfile));
    goto cleanup;
  }

  if (pgp_use_gpg_agent())
    *PgpPass = '\0';
  if (flags & SEC_SIGN)
    fprintf(fp_pgp_in, "%s\n", PgpPass);
  mutt_file_fclose(&fp_pgp_in);

  if (filter_wait(pid) && C_PgpCheckExit)
    empty = true;

  mutt_file_unlink(mutt_b2s(pgpinfile));

  fflush(fp_pgp_out);
  fflush(fp_pgp_err);

  rewind(fp_pgp_out);
  rewind(fp_pgp_err);

  if (!empty)
    empty = (fgetc(fp_pgp_out) == EOF);
  mutt_file_fclose(&fp_pgp_out);

  err = false;

  while (fgets(buf, sizeof(buf), fp_pgp_err))
  {
    err = true;
    fputs(buf, stdout);
  }

  mutt_file_fclose(&fp_pgp_err);

  if (err)
    mutt_any_key_to_continue(NULL);

  if (empty)
  {
    if (flags & SEC_SIGN)
      pgp_class_void_passphrase(); /* just in case */
    unlink(mutt_b2s(pgpoutfile));
    goto cleanup;
  }

  b = mutt_body_new();

  b->encoding = ENC_7BIT;

  b->type = TYPE_TEXT;
  b->subtype = mutt_str_dup("plain");

  mutt_param_set(&b->parameter, "x-action",
                 (flags & SEC_ENCRYPT) ? "pgp-encrypted" : "pgp-signed");
  mutt_param_set(&b->parameter, "charset", send_charset);

  b->filename = mutt_buffer_strdup(pgpoutfile);

  b->disposition = DISP_NONE;
  b->unlink = true;

  b->noconv = true;
  b->use_disp = false;

  if (!(flags & SEC_ENCRYPT))
    b->encoding = a->encoding;

cleanup:
  mutt_buffer_pool_release(&pgpinfile);
  mutt_buffer_pool_release(&pgpoutfile);
  return b;
}

/**
 * pgp_class_send_menu - Implements CryptModuleSpecs::send_menu()
 */
int pgp_class_send_menu(struct Email *e)
{
  struct PgpKeyInfo *p = NULL;
  const char *prompt = NULL;
  const char *letters = NULL;
  const char *choices = NULL;
  char promptbuf[1024];
  int choice;

  if (!(WithCrypto & APPLICATION_PGP))
    return e->security;

  /* If autoinline and no crypto options set, then set inline. */
  if (C_PgpAutoinline &&
      !((e->security & APPLICATION_PGP) && (e->security & (SEC_SIGN | SEC_ENCRYPT))))
  {
    e->security |= SEC_INLINE;
  }

  e->security |= APPLICATION_PGP;

  char *mime_inline = NULL;
  if (e->security & SEC_INLINE)
  {
    /* L10N: The next string MUST have the same highlighted letter
       One of them will appear in each of the three strings marked "(inline"), below. */
    mime_inline = _("PGP/M(i)ME");
  }
  else
  {
    /* L10N: The previous string MUST have the same highlighted letter
       One of them will appear in each of the three strings marked "(inline"), below. */
    mime_inline = _("(i)nline");
  }
  /* Opportunistic encrypt is controlling encryption.  Allow to toggle
   * between inline and mime, but not turn encryption on or off.
   * NOTE: "Signing" and "Clearing" only adjust the sign bit, so we have different
   *       letter choices for those.  */
  if (C_CryptOpportunisticEncrypt && (e->security & SEC_OPPENCRYPT))
  {
    if (e->security & (SEC_ENCRYPT | SEC_SIGN))
    {
      snprintf(promptbuf, sizeof(promptbuf),
               /* L10N: PGP options (inline) (opportunistic encryption is on) */
               _("PGP (s)ign, sign (a)s, %s format, (c)lear, or (o)ppenc mode "
                 "off?"),
               mime_inline);
      prompt = promptbuf;
      /* L10N: PGP options (inline) (opportunistic encryption is on)
         The 'i' is from the "PGP/M(i)ME" or "(i)nline", above. */
      letters = _("saico");
      choices = "SaiCo";
    }
    else
    {
      /* L10N: PGP options (opportunistic encryption is on) */
      prompt = _("PGP (s)ign, sign (a)s, (c)lear, or (o)ppenc mode off?");
      /* L10N: PGP options (opportunistic encryption is on) */
      letters = _("saco");
      choices = "SaCo";
    }
  }
  /* Opportunistic encryption option is set, but is toggled off
   * for this message.  */
  else if (C_CryptOpportunisticEncrypt)
  {
    /* When the message is not selected for signing or encryption, the toggle
     * between PGP/MIME and Traditional doesn't make sense.  */
    if (e->security & (SEC_ENCRYPT | SEC_SIGN))
    {
      snprintf(promptbuf, sizeof(promptbuf),
               /* L10N: PGP options (inline) (opportunistic encryption is off) */
               _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, %s format, "
                 "(c)lear, or (o)ppenc mode?"),
               mime_inline);
      prompt = promptbuf;
      /* L10N: PGP options (inline) (opportunistic encryption is off)
         The 'i' is from the "PGP/M(i)ME" or "(i)nline", above. */
      letters = _("esabico");
      choices = "esabicO";
    }
    else
    {
      /* L10N: PGP options (opportunistic encryption is off) */
      prompt = _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (c)lear, or "
                 "(o)ppenc mode?");
      /* L10N: PGP options (opportunistic encryption is off) */
      letters = _("esabco");
      choices = "esabcO";
    }
  }
  /* Opportunistic encryption is unset */
  else
  {
    if (e->security & (SEC_ENCRYPT | SEC_SIGN))
    {
      snprintf(promptbuf, sizeof(promptbuf),
               /* L10N: PGP options (inline) */
               _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, %s format, or "
                 "(c)lear?"),
               mime_inline);
      prompt = promptbuf;
      /* L10N: PGP options (inline)
         The 'i' is from the "PGP/M(i)ME" or "(i)nline", above. */
      letters = _("esabic");
      choices = "esabic";
    }
    else
    {
      /* L10N: PGP options */
      prompt = _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (c)lear?");
      /* L10N: PGP options */
      letters = _("esabc");
      choices = "esabc";
    }
  }

  choice = mutt_multi_choice(prompt, letters);
  if (choice > 0)
  {
    switch (choices[choice - 1])
    {
      case 'a': /* sign (a)s */
        OptPgpCheckTrust = false;

        p = pgp_ask_for_key(_("Sign as: "), NULL, KEYFLAG_NO_FLAGS, PGP_SECRING);
        if (p)
        {
          char input_signas[128];
          snprintf(input_signas, sizeof(input_signas), "0x%s", pgp_fpr_or_lkeyid(p));
          mutt_str_replace(&C_PgpSignAs, input_signas);
          pgp_key_free(&p);

          e->security |= SEC_SIGN;

          crypt_pgp_void_passphrase(); /* probably need a different passphrase */
        }
        break;

      case 'b': /* (b)oth */
        e->security |= (SEC_ENCRYPT | SEC_SIGN);
        break;

      case 'C':
        e->security &= ~SEC_SIGN;
        break;

      case 'c': /* (c)lear     */
        e->security &= ~(SEC_ENCRYPT | SEC_SIGN);
        break;

      case 'e': /* (e)ncrypt */
        e->security |= SEC_ENCRYPT;
        e->security &= ~SEC_SIGN;
        break;

      case 'i': /* toggle (i)nline */
        e->security ^= SEC_INLINE;
        break;

      case 'O': /* oppenc mode on */
        e->security |= SEC_OPPENCRYPT;
        crypt_opportunistic_encrypt(e);
        break;

      case 'o': /* oppenc mode off */
        e->security &= ~SEC_OPPENCRYPT;
        break;

      case 'S': /* (s)ign in oppenc mode */
        e->security |= SEC_SIGN;
        break;

      case 's': /* (s)ign */
        e->security &= ~SEC_ENCRYPT;
        e->security |= SEC_SIGN;
        break;
    }
  }

  return e->security;
}
