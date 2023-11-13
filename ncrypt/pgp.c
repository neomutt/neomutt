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
 * Code to sign, encrypt, verify and decrypt PGP messages.
 *
 * The code accepts messages in either the new PGP/MIME format, or in the older
 * Application/Pgp format.  It also contains some code to cache the user's
 * passphrase for repeat use when decrypting or signing a message.
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "attach/lib.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "crypt.h"
#include "cryptglue.h"
#include "globals.h"
#include "handler.h"
#include "hook.h"
#include "pgpinvoke.h"
#include "pgpkey.h"
#include "pgpmicalg.h"
#ifdef CRYPT_BACKEND_CLASSIC_PGP
#include "pgp.h"
#include "pgplib.h"
#endif

/// Cached PGP Passphrase
static char PgpPass[1024];
/// Unix time when #PgpPass expires
static time_t PgpExptime = 0; /* when does the cached passphrase expire? */

/**
 * pgp_class_void_passphrase - Forget the cached passphrase - Implements CryptModuleSpecs::void_passphrase() - @ingroup crypto_void_passphrase
 */
void pgp_class_void_passphrase(void)
{
  memset(PgpPass, 0, sizeof(PgpPass));
  PgpExptime = 0;
}

/**
 * pgp_class_valid_passphrase - Ensure we have a valid passphrase - Implements CryptModuleSpecs::valid_passphrase() - @ingroup crypto_valid_passphrase
 */
bool pgp_class_valid_passphrase(void)
{
  if (pgp_use_gpg_agent())
  {
    *PgpPass = '\0';
    return true; /* handled by gpg-agent */
  }

  if (mutt_date_now() < PgpExptime)
  {
    /* Use cached copy.  */
    return true;
  }

  pgp_class_void_passphrase();

  struct Buffer *buf = buf_pool_get();
  const int rc = mw_get_field(_("Enter PGP passphrase:"), buf,
                              MUTT_COMP_PASS | MUTT_COMP_UNBUFFERED, HC_OTHER, NULL, NULL);
  mutt_str_copy(PgpPass, buf_string(buf), sizeof(PgpPass));
  buf_pool_release(&buf);

  if (rc == 0)
  {
    const long c_pgp_timeout = cs_subset_long(NeoMutt->sub, "pgp_timeout");
    PgpExptime = mutt_date_add_timeout(mutt_date_now(), c_pgp_timeout);
    return true;
  }
  else
  {
    PgpExptime = 0;
  }

  return false;
}

/**
 * pgp_use_gpg_agent - Does the user want to use the gpg agent?
 * @retval true The user wants to use the gpg agent
 *
 * @note This functions sets the environment variable `$GPG_TTY`
 */
bool pgp_use_gpg_agent(void)
{
  char *tty = NULL;

  /* GnuPG 2.1 no longer exports GPG_AGENT_INFO */
  const bool c_pgp_use_gpg_agent = cs_subset_bool(NeoMutt->sub, "pgp_use_gpg_agent");
  if (!c_pgp_use_gpg_agent)
    return false;

  tty = ttyname(0);
  if (tty)
  {
    setenv("GPG_TTY", tty, 0);
    envlist_set(&EnvList, "GPG_TTY", tty, false);
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
  const bool c_pgp_ignore_subkeys = cs_subset_bool(NeoMutt->sub, "pgp_ignore_subkeys");
  if ((k->flags & KEYFLAG_SUBKEY) && k->parent && c_pgp_ignore_subkeys)
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
  const bool c_pgp_long_ids = cs_subset_bool(NeoMutt->sub, "pgp_long_ids");
  if (c_pgp_long_ids)
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

  const struct Regex *c_pgp_good_sign = cs_subset_regex(NeoMutt->sub, "pgp_good_sign");
  if (c_pgp_good_sign && c_pgp_good_sign->regex)
  {
    char *line = NULL;
    size_t linelen;

    while ((line = mutt_file_read_line(line, &linelen, fp_in, NULL, MUTT_RL_NO_FLAGS)))
    {
      if (mutt_regex_match(c_pgp_good_sign, line))
      {
        mutt_debug(LL_DEBUG2, "\"%s\" matches regex\n", line);
        rc = 0;
      }
      else
      {
        mutt_debug(LL_DEBUG2, "\"%s\" doesn't match regex\n", line);
      }

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

  const struct Regex *c_pgp_decryption_okay = cs_subset_regex(NeoMutt->sub, "pgp_decryption_okay");
  if (c_pgp_decryption_okay && c_pgp_decryption_okay->regex)
  {
    char *line = NULL;
    size_t linelen;

    while ((line = mutt_file_read_line(line, &linelen, fp_in, NULL, MUTT_RL_NO_FLAGS)))
    {
      if (mutt_regex_match(c_pgp_decryption_okay, line))
      {
        mutt_debug(LL_DEBUG2, "\"%s\" matches regex\n", line);
        rc = 0;
        break;
      }
      else
      {
        mutt_debug(LL_DEBUG2, "\"%s\" doesn't match regex\n", line);
      }
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

  const bool c_pgp_check_gpg_decrypt_status_fd = cs_subset_bool(NeoMutt->sub, "pgp_check_gpg_decrypt_status_fd");
  if (!c_pgp_check_gpg_decrypt_status_fd)
    return pgp_check_pgp_decryption_okay_regex(fp_in);

  while ((line = mutt_file_read_line(line, &linelen, fp_in, NULL, MUTT_RL_NO_FLAGS)))
  {
    size_t plen = mutt_str_startswith(line, "[GNUPG:] ");
    if (plen == 0)
      continue;
    s = line + plen;
    mutt_debug(LL_DEBUG2, "checking \"%s\"\n", line);
    if (mutt_str_startswith(s, "BEGIN_DECRYPTION"))
    {
      inside_decrypt = 1;
    }
    else if (mutt_str_startswith(s, "END_DECRYPTION"))
    {
      inside_decrypt = 0;
    }
    else if (mutt_str_startswith(s, "PLAINTEXT"))
    {
      if (!inside_decrypt)
      {
        mutt_debug(LL_DEBUG2, "    PLAINTEXT encountered outside of DECRYPTION\n");
        rc = -2;
        break;
      }
    }
    else if (mutt_str_startswith(s, "DECRYPTION_FAILED"))
    {
      mutt_debug(LL_DEBUG2, "    DECRYPTION_FAILED encountered.  Failure\n");
      rc = -3;
      break;
    }
    else if (mutt_str_startswith(s, "DECRYPTION_OKAY"))
    {
      /* Don't break out because we still have to check for
       * PLAINTEXT outside of the decryption boundaries. */
      mutt_debug(LL_DEBUG2, "    DECRYPTION_OKAY encountered\n");
      rc = 0;
    }
  }
  FREE(&line);

  return rc;
}

/**
 * pgp_copy_clearsigned - Copy a clearsigned message, stripping the signature
 * @param fp_in   File to read from
 * @param state   State to use
 * @param charset Charset of file
 *
 * XXX charset handling: We assume that it is safe to do character set
 * decoding first, dash decoding second here, while we do it the other way
 * around in the main handler.
 *
 * (Note that we aren't worse than Outlook &c in this, and also note that we
 * can successfully handle anything produced by any existing versions of neomutt.)
 */
static void pgp_copy_clearsigned(FILE *fp_in, struct State *state, char *charset)
{
  char buf[8192] = { 0 };
  bool complete, armor_header;

  rewind(fp_in);

  /* fromcode comes from the MIME Content-Type charset label. It might
   * be a wrong label, so we want the ability to do corrections via
   * charset-hooks. Therefore we set flags to MUTT_ICONV_HOOK_FROM.  */
  struct FgetConv *fc = mutt_ch_fgetconv_open(fp_in, charset, cc_charset(), MUTT_ICONV_HOOK_FROM);

  for (complete = true, armor_header = true;
       mutt_ch_fgetconvs(buf, sizeof(buf), fc); complete = (strchr(buf, '\n')))
  {
    if (!complete)
    {
      if (!armor_header)
        state_puts(state, buf);
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

    if (state->prefix)
      state_puts(state, state->prefix);

    if ((buf[0] == '-') && (buf[1] == ' '))
      state_puts(state, buf + 2);
    else
      state_puts(state, buf);
  }

  mutt_ch_fgetconv_close(&fc);
}

/**
 * pgp_class_application_handler - Manage the MIME type "application/pgp" or "application/smime" - Implements CryptModuleSpecs::application_handler() - @ingroup crypto_application_handler
 */
int pgp_class_application_handler(struct Body *b, struct State *state)
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
  char buf[8192] = { 0 };
  FILE *fp_pgp_out = NULL, *fp_pgp_in = NULL, *fp_pgp_err = NULL;
  FILE *fp_tmp = NULL;
  pid_t pid;
  struct Buffer *tmpfname = buf_pool_get();

  bool maybe_goodsig = true;
  bool have_any_sigs = false;

  char *gpgcharset = NULL;
  char body_charset[256] = { 0 };
  mutt_body_get_charset(b, body_charset, sizeof(body_charset));

  if (!mutt_file_seek(state->fp_in, b->offset, SEEK_SET))
  {
    return -1;
  }
  last_pos = b->offset;

  for (bytes = b->length; bytes > 0;)
  {
    if (!fgets(buf, sizeof(buf), state->fp_in))
      break;

    offset = ftello(state->fp_in);
    bytes -= (offset - last_pos); /* don't rely on mutt_str_len(buf) */
    last_pos = offset;

    size_t plen = mutt_str_startswith(buf, "-----BEGIN PGP ");
    if (plen != 0)
    {
      clearsign = false;
      could_not_decrypt = false;
      decrypt_okay_rc = 0;

      if (mutt_str_startswith(buf + plen, "MESSAGE-----\n"))
      {
        needpass = 1;
      }
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
        if (state->prefix)
          state_puts(state, state->prefix);
        state_puts(state, buf);
        continue;
      }

      have_any_sigs = have_any_sigs || (clearsign && (state->flags & STATE_VERIFY));

      /* Copy PGP material to temporary file */
      buf_mktemp(tmpfname);
      fp_tmp = mutt_file_fopen(buf_string(tmpfname), "w+");
      if (!fp_tmp)
      {
        mutt_perror("%s", buf_string(tmpfname));
        FREE(&gpgcharset);
        goto out;
      }

      fputs(buf, fp_tmp);
      while ((bytes > 0) && fgets(buf, sizeof(buf) - 1, state->fp_in))
      {
        offset = ftello(state->fp_in);
        bytes -= (offset - last_pos); /* don't rely on mutt_str_len(buf) */
        last_pos = offset;

        fputs(buf, fp_tmp);

        if ((needpass && mutt_str_equal("-----END PGP MESSAGE-----\n", buf)) ||
            (!needpass && (mutt_str_equal("-----END PGP SIGNATURE-----\n", buf) ||
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
      if (!clearsign || (state->flags & STATE_VERIFY))
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
                                fileno(fp_pgp_err), buf_string(tmpfname),
                                (needpass != 0));
        if (pid == -1)
        {
          mutt_file_fclose(&fp_pgp_out);
          maybe_goodsig = false;
          fp_pgp_in = NULL;
          state_attach_puts(state, _("[-- Error: unable to create PGP subprocess --]\n"));
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

          if (state->flags & STATE_DISPLAY)
          {
            rewind(fp_pgp_err);
            crypt_current_time(state, "PGP");
            int checksig_rc = pgp_copy_checksig(fp_pgp_err, state->fp_out);

            if (checksig_rc == 0)
              have_any_sigs = true;
            /* Sig is bad if
             * gpg_good_sign-pattern did not match || pgp_decode_command returned not 0
             * Sig _is_ correct if
             *  gpg_good_sign="" && pgp_decode_command returned 0 */
            if (checksig_rc == -1 || (wait_filter_rc != 0))
              maybe_goodsig = false;

            state_attach_puts(state, _("[-- End of PGP output --]\n\n"));
          }
          if (pgp_use_gpg_agent())
          {
            mutt_need_hard_redraw();
          }
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

        if ((could_not_decrypt || (decrypt_okay_rc <= -3)) && !(state->flags & STATE_DISPLAY))
        {
          mutt_error(_("Could not decrypt PGP message"));
          goto out;
        }
      }

      /* Now, copy cleartext to the screen.  */
      if (state->flags & STATE_DISPLAY)
      {
        if (needpass)
          state_attach_puts(state, _("[-- BEGIN PGP MESSAGE --]\n\n"));
        else if (pgp_keyblock)
          state_attach_puts(state, _("[-- BEGIN PGP PUBLIC KEY BLOCK --]\n"));
        else
          state_attach_puts(state, _("[-- BEGIN PGP SIGNED MESSAGE --]\n\n"));
      }

      if (clearsign)
      {
        rewind(fp_tmp);
        pgp_copy_clearsigned(fp_tmp, state, body_charset);
      }
      else if (fp_pgp_out)
      {
        struct FgetConv *fc = NULL;
        int ch;
        char *expected_charset = (gpgcharset && *gpgcharset) ? gpgcharset : "utf-8";

        mutt_debug(LL_DEBUG3, "pgp: recoding inline from [%s] to [%s]\n",
                   expected_charset, cc_charset());

        rewind(fp_pgp_out);
        state_set_prefix(state);
        fc = mutt_ch_fgetconv_open(fp_pgp_out, expected_charset, cc_charset(),
                                   MUTT_ICONV_HOOK_FROM);
        while ((ch = mutt_ch_fgetconv(fc)) != EOF)
          state_prefix_putc(state, ch);
        mutt_ch_fgetconv_close(&fc);
      }

      /* Multiple PGP blocks can exist, so these need to be closed and
       * unlinked inside the loop.  */
      mutt_file_fclose(&fp_tmp);
      mutt_file_unlink(buf_string(tmpfname));
      mutt_file_fclose(&fp_pgp_out);
      mutt_file_fclose(&fp_pgp_err);

      if (state->flags & STATE_DISPLAY)
      {
        state_putc(state, '\n');
        if (needpass)
        {
          state_attach_puts(state, _("[-- END PGP MESSAGE --]\n"));
          if (could_not_decrypt || (decrypt_okay_rc <= -3))
          {
            mutt_error(_("Could not decrypt PGP message"));
          }
          else if (decrypt_okay_rc < 0)
          {
            /* L10N: You will see this error message if (1) you are decrypting
               (not encrypting) something and (2) it is a plaintext. So the
               message does not mean "You failed to encrypt the message." */
            mutt_error(_("PGP message is not encrypted"));
          }
          else
          {
            mutt_message(_("PGP message successfully decrypted"));
          }
        }
        else if (pgp_keyblock)
        {
          state_attach_puts(state, _("[-- END PGP PUBLIC KEY BLOCK --]\n"));
        }
        else
        {
          state_attach_puts(state, _("[-- END PGP SIGNED MESSAGE --]\n"));
        }
      }
    }
    else
    {
      /* A traditional PGP part may mix signed and unsigned content */
      /* XXX we may wish to recode here */
      if (state->prefix)
        state_puts(state, state->prefix);
      state_puts(state, buf);
    }
  }

  rc = 0;

out:
  b->goodsig = (maybe_goodsig && have_any_sigs);

  if (fp_tmp)
  {
    mutt_file_fclose(&fp_tmp);
    mutt_file_unlink(buf_string(tmpfname));
  }
  mutt_file_fclose(&fp_pgp_out);
  mutt_file_fclose(&fp_pgp_err);

  buf_pool_release(&tmpfname);

  FREE(&gpgcharset);

  if (needpass == -1)
  {
    state_attach_puts(state, _("[-- Error: could not find beginning of PGP message --]\n\n"));
    return -1;
  }

  return rc;
}

/**
 * pgp_check_traditional_one_body - Check the body of an inline PGP message
 * @param fp File to read
 * @param b  Body to populate
 * @retval true  Success
 * @retval false Error
 */
static bool pgp_check_traditional_one_body(FILE *fp, struct Body *b)
{
  struct Buffer *tempfile = NULL;
  char buf[8192] = { 0 };
  bool rc = false;

  bool sgn = false;
  bool enc = false;
  bool key = false;

  if (b->type != TYPE_TEXT)
    goto cleanup;

  tempfile = buf_pool_get();
  buf_mktemp(tempfile);
  if (mutt_decode_save_attachment(fp, b, buf_string(tempfile), STATE_NO_FLAGS,
                                  MUTT_SAVE_NO_FLAGS) != 0)
  {
    unlink(buf_string(tempfile));
    goto cleanup;
  }

  FILE *fp_tmp = fopen(buf_string(tempfile), "r");
  if (!fp_tmp)
  {
    unlink(buf_string(tempfile));
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
  unlink(buf_string(tempfile));

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

  rc = true;

cleanup:
  buf_pool_release(&tempfile);
  return rc;
}

/**
 * pgp_class_check_traditional - Look for inline (non-MIME) PGP content - Implements CryptModuleSpecs::pgp_check_traditional() - @ingroup crypto_pgp_check_traditional
 */
bool pgp_class_check_traditional(FILE *fp, struct Body *b, bool just_one)
{
  bool rc = false;
  int r;
  for (; b; b = b->next)
  {
    if (!just_one && is_multipart(b))
    {
      rc = pgp_class_check_traditional(fp, b->parts, false) || rc;
    }
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
 * pgp_class_verify_one - Check a signed MIME part against a signature - Implements CryptModuleSpecs::verify_one() - @ingroup crypto_verify_one
 */
int pgp_class_verify_one(struct Body *b, struct State *state, const char *tempfile)
{
  FILE *fp_pgp_out = NULL;
  pid_t pid;
  int badsig = -1;
  struct Buffer *sigfile = buf_pool_get();

  buf_printf(sigfile, "%s.asc", tempfile);

  FILE *fp_sig = mutt_file_fopen(buf_string(sigfile), "w");
  if (!fp_sig)
  {
    mutt_perror("%s", buf_string(sigfile));
    goto cleanup;
  }

  if (!mutt_file_seek(state->fp_in, b->offset, SEEK_SET))
  {
    mutt_file_fclose(&fp_sig);
    goto cleanup;
  }
  mutt_file_copy_bytes(state->fp_in, fp_sig, b->length);
  mutt_file_fclose(&fp_sig);

  FILE *fp_pgp_err = mutt_file_mkstemp();
  if (!fp_pgp_err)
  {
    mutt_perror(_("Can't create temporary file"));
    unlink(buf_string(sigfile));
    goto cleanup;
  }

  crypt_current_time(state, "PGP");

  pid = pgp_invoke_verify(NULL, &fp_pgp_out, NULL, -1, -1, fileno(fp_pgp_err),
                          tempfile, buf_string(sigfile));
  if (pid != -1)
  {
    if (pgp_copy_checksig(fp_pgp_out, state->fp_out) >= 0)
      badsig = 0;

    mutt_file_fclose(&fp_pgp_out);
    fflush(fp_pgp_err);
    rewind(fp_pgp_err);

    if (pgp_copy_checksig(fp_pgp_err, state->fp_out) >= 0)
      badsig = 0;

    const int rv = filter_wait(pid);
    if (rv)
      badsig = -1;

    mutt_debug(LL_DEBUG1, "filter_wait returned %d\n", rv);
  }

  mutt_file_fclose(&fp_pgp_err);

  state_attach_puts(state, _("[-- End of PGP output --]\n\n"));

  mutt_file_unlink(buf_string(sigfile));

cleanup:
  buf_pool_release(&sigfile);

  mutt_debug(LL_DEBUG1, "returning %d\n", badsig);
  return badsig;
}

/**
 * pgp_extract_keys_from_attachment - Extract pgp keys from messages/attachments
 * @param fp  File to read from
 * @param b   Top Attachment
 */
static void pgp_extract_keys_from_attachment(FILE *fp, struct Body *b)
{
  struct State state = { 0 };
  struct Buffer *tempfname = buf_pool_get();

  buf_mktemp(tempfname);
  FILE *fp_tmp = mutt_file_fopen(buf_string(tempfname), "w");
  if (!fp_tmp)
  {
    mutt_perror("%s", buf_string(tempfname));
    goto cleanup;
  }

  state.fp_in = fp;
  state.fp_out = fp_tmp;

  mutt_body_handler(b, &state);

  mutt_file_fclose(&fp_tmp);

  pgp_class_invoke_import(buf_string(tempfname));
  mutt_any_key_to_continue(NULL);

  mutt_file_unlink(buf_string(tempfname));

cleanup:
  buf_pool_release(&tempfname);
}

/**
 * pgp_class_extract_key_from_attachment - Extract PGP key from an attachment - Implements CryptModuleSpecs::pgp_extract_key_from_attachment() - @ingroup crypto_pgp_extract_key_from_attachment
 */
void pgp_class_extract_key_from_attachment(FILE *fp, struct Body *b)
{
  if (!fp)
  {
    mutt_error(_("Internal error.  Please submit a bug report."));
    return;
  }

  mutt_endwin();

  OptDontHandlePgpKeys = true;
  pgp_extract_keys_from_attachment(fp, b);
  OptDontHandlePgpKeys = false;
}

/**
 * pgp_decrypt_part - Decrypt part of a PGP message
 * @param a      Body of attachment
 * @param state  State to use
 * @param fp_out File to write to
 * @param p      Body of parent (main email)
 * @retval ptr New Body for the attachment
 */
static struct Body *pgp_decrypt_part(struct Body *a, struct State *state,
                                     FILE *fp_out, struct Body *p)
{
  if (!a || !state || !fp_out || !p)
    return NULL;

  char buf[1024] = { 0 };
  FILE *fp_pgp_in = NULL, *fp_pgp_out = NULL, *fp_pgp_tmp = NULL;
  struct Body *tattach = NULL;
  pid_t pid;
  int rv;
  struct Buffer *pgptmpfile = buf_pool_get();

  FILE *fp_pgp_err = mutt_file_mkstemp();
  if (!fp_pgp_err)
  {
    mutt_perror(_("Can't create temporary file"));
    goto cleanup;
  }

  buf_mktemp(pgptmpfile);
  fp_pgp_tmp = mutt_file_fopen(buf_string(pgptmpfile), "w");
  if (!fp_pgp_tmp)
  {
    mutt_perror("%s", buf_string(pgptmpfile));
    mutt_file_fclose(&fp_pgp_err);
    goto cleanup;
  }

  /* Position the stream at the beginning of the body, and send the data to
   * the temporary file.  */

  if (!mutt_file_seek(state->fp_in, a->offset, SEEK_SET))
  {
    mutt_file_fclose(&fp_pgp_tmp);
    mutt_file_fclose(&fp_pgp_err);
    goto cleanup;
  }
  mutt_file_copy_bytes(state->fp_in, fp_pgp_tmp, a->length);
  mutt_file_fclose(&fp_pgp_tmp);

  pid = pgp_invoke_decrypt(&fp_pgp_in, &fp_pgp_out, NULL, -1, -1,
                           fileno(fp_pgp_err), buf_string(pgptmpfile));
  if (pid == -1)
  {
    mutt_file_fclose(&fp_pgp_err);
    unlink(buf_string(pgptmpfile));
    if (state->flags & STATE_DISPLAY)
    {
      state_attach_puts(state, _("[-- Error: could not create a PGP subprocess --]\n\n"));
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
  const bool c_pgp_use_gpg_agent = cs_subset_bool(NeoMutt->sub, "pgp_use_gpg_agent");
  if (c_pgp_use_gpg_agent)
    mutt_need_hard_redraw();

  mutt_file_unlink(buf_string(pgptmpfile));

  fflush(fp_pgp_err);
  rewind(fp_pgp_err);
  if (pgp_check_decryption_okay(fp_pgp_err) < 0)
  {
    mutt_error(_("Decryption failed"));
    pgp_class_void_passphrase();
    mutt_file_fclose(&fp_pgp_err);
    goto cleanup;
  }

  if (state->flags & STATE_DISPLAY)
  {
    rewind(fp_pgp_err);
    if ((pgp_copy_checksig(fp_pgp_err, state->fp_out) == 0) && !rv)
      p->goodsig = true;
    else
      p->goodsig = false;
    state_attach_puts(state, _("[-- End of PGP output --]\n\n"));
  }
  mutt_file_fclose(&fp_pgp_err);

  fflush(fp_out);
  rewind(fp_out);

  if (fgetc(fp_out) == EOF)
  {
    mutt_error(_("Decryption failed"));
    pgp_class_void_passphrase();
    goto cleanup;
  }

  rewind(fp_out);
  const long size = mutt_file_get_size_fp(fp_out);
  if (size == 0)
  {
    goto cleanup;
  }

  tattach = mutt_read_mime_header(fp_out, 0);
  if (tattach)
  {
    /* Need to set the length of this body part.  */
    tattach->length = size - tattach->offset;

    /* See if we need to recurse on this MIME part.  */
    mutt_parse_part(fp_out, tattach);
  }

cleanup:
  buf_pool_release(&pgptmpfile);
  return tattach;
}

/**
 * pgp_class_decrypt_mime - Decrypt an encrypted MIME part - Implements CryptModuleSpecs::decrypt_mime() - @ingroup crypto_decrypt_mime
 */
int pgp_class_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **b_dec)
{
  struct State state = { 0 };
  struct Body *p = b;
  bool need_decode = false;
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
  {
    return -1;
  }

  state.fp_in = fp_in;

  if (need_decode)
  {
    saved_offset = b->offset;
    saved_length = b->length;

    fp_decoded = mutt_file_mkstemp();
    if (!fp_decoded)
    {
      mutt_perror(_("Can't create temporary file"));
      return -1;
    }

    if (!mutt_file_seek(state.fp_in, b->offset, SEEK_SET))
    {
      rc = -1;
      goto bail;
    }
    state.fp_out = fp_decoded;

    mutt_decode_attachment(b, &state);

    fflush(fp_decoded);
    b->length = ftello(fp_decoded);
    b->offset = 0;
    rewind(fp_decoded);
    state.fp_in = fp_decoded;
    state.fp_out = 0;
  }

  *fp_out = mutt_file_mkstemp();
  if (!*fp_out)
  {
    mutt_perror(_("Can't create temporary file"));
    rc = -1;
    goto bail;
  }

  *b_dec = pgp_decrypt_part(b, &state, *fp_out, p);
  if (!*b_dec)
    rc = -1;
  rewind(*fp_out);

bail:
  if (need_decode)
  {
    b->length = saved_length;
    b->offset = saved_offset;
    mutt_file_fclose(&fp_decoded);
  }

  return rc;
}

/**
 * pgp_class_encrypted_handler - Manage a PGP or S/MIME encrypted MIME part - Implements CryptModuleSpecs::encrypted_handler() - @ingroup crypto_encrypted_handler
 */
int pgp_class_encrypted_handler(struct Body *b, struct State *state)
{
  FILE *fp_in = NULL;
  struct Body *tattach = NULL;
  int rc = 0;

  FILE *fp_out = mutt_file_mkstemp();
  if (!fp_out)
  {
    mutt_perror(_("Can't create temporary file"));
    if (state->flags & STATE_DISPLAY)
    {
      state_attach_puts(state, _("[-- Error: could not create temporary file --]\n"));
    }
    return -1;
  }

  if (state->flags & STATE_DISPLAY)
    crypt_current_time(state, "PGP");

  tattach = pgp_decrypt_part(b, state, fp_out, b);
  if (tattach)
  {
    if (state->flags & STATE_DISPLAY)
    {
      state_attach_puts(state, _("[-- The following data is PGP/MIME encrypted --]\n\n"));
      mutt_protected_headers_handler(tattach, state);
    }

    /* Store any protected headers in the parent so they can be
     * accessed for index updates after the handler recursion is done.
     * This is done before the handler to prevent a nested encrypted
     * handler from freeing the headers. */
    mutt_env_free(&b->mime_headers);
    b->mime_headers = tattach->mime_headers;
    tattach->mime_headers = NULL;

    fp_in = state->fp_in;
    state->fp_in = fp_out;
    rc = mutt_body_handler(tattach, state);
    state->fp_in = fp_in;

    /* Embedded multipart signed protected headers override the
     * encrypted headers.  We need to do this after the handler so
     * they can be printed in the pager. */
    if (mutt_is_multipart_signed(tattach) && tattach->parts && tattach->parts->mime_headers)
    {
      mutt_env_free(&b->mime_headers);
      b->mime_headers = tattach->parts->mime_headers;
      tattach->parts->mime_headers = NULL;
    }

    /* if a multipart/signed is the _only_ sub-part of a
     * multipart/encrypted, cache signature verification
     * status.  */
    if (mutt_is_multipart_signed(tattach) && !tattach->next)
      b->goodsig |= tattach->goodsig;

    if (state->flags & STATE_DISPLAY)
    {
      state_puts(state, "\n");
      state_attach_puts(state, _("[-- End of PGP/MIME encrypted data --]\n"));
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
 * pgp_class_sign_message - Cryptographically sign the Body of a message - Implements CryptModuleSpecs::sign_message() - @ingroup crypto_sign_message
 */
struct Body *pgp_class_sign_message(struct Body *b, const struct AddressList *from)
{
  struct Body *b_enc = NULL, *rv = NULL;
  char buf[1024] = { 0 };
  FILE *fp_pgp_in = NULL, *fp_pgp_out = NULL, *fp_pgp_err = NULL, *fp_signed = NULL;
  bool err = false;
  bool empty = true;
  pid_t pid;
  struct Buffer *sigfile = buf_pool_get();
  struct Buffer *signedfile = buf_pool_get();

  crypt_convert_to_7bit(b); /* Signed data _must_ be in 7-bit format. */

  buf_mktemp(sigfile);
  FILE *fp_sig = mutt_file_fopen(buf_string(sigfile), "w");
  if (!fp_sig)
  {
    goto cleanup;
  }

  buf_mktemp(signedfile);
  fp_signed = mutt_file_fopen(buf_string(signedfile), "w");
  if (!fp_signed)
  {
    mutt_perror("%s", buf_string(signedfile));
    mutt_file_fclose(&fp_sig);
    unlink(buf_string(sigfile));
    goto cleanup;
  }

  mutt_write_mime_header(b, fp_signed, NeoMutt->sub);
  fputc('\n', fp_signed);
  mutt_write_mime_body(b, fp_signed, NeoMutt->sub);
  mutt_file_fclose(&fp_signed);

  pid = pgp_invoke_sign(&fp_pgp_in, &fp_pgp_out, &fp_pgp_err, -1, -1, -1,
                        buf_string(signedfile));
  if (pid == -1)
  {
    mutt_perror(_("Can't open PGP subprocess"));
    mutt_file_fclose(&fp_sig);
    unlink(buf_string(sigfile));
    unlink(buf_string(signedfile));
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

  const bool c_pgp_check_exit = cs_subset_bool(NeoMutt->sub, "pgp_check_exit");
  if (filter_wait(pid) && c_pgp_check_exit)
    empty = true;

  mutt_file_fclose(&fp_pgp_err);
  mutt_file_fclose(&fp_pgp_out);
  unlink(buf_string(signedfile));

  if (mutt_file_fclose(&fp_sig) != 0)
  {
    mutt_perror("fclose");
    unlink(buf_string(sigfile));
    goto cleanup;
  }

  if (err)
    mutt_any_key_to_continue(NULL);
  if (empty)
  {
    unlink(buf_string(sigfile));
    /* most likely error is a bad passphrase, so automatically forget it */
    pgp_class_void_passphrase();
    goto cleanup; /* fatal error while signing */
  }

  b_enc = mutt_body_new();
  b_enc->type = TYPE_MULTIPART;
  b_enc->subtype = mutt_str_dup("signed");
  b_enc->encoding = ENC_7BIT;
  b_enc->use_disp = false;
  b_enc->disposition = DISP_INLINE;
  rv = b_enc;

  mutt_generate_boundary(&b_enc->parameter);
  mutt_param_set(&b_enc->parameter, "protocol", "application/pgp-signature");
  mutt_param_set(&b_enc->parameter, "micalg", pgp_micalg(buf_string(sigfile)));

  b_enc->parts = b;

  b_enc->parts->next = mutt_body_new();
  b_enc = b_enc->parts->next;
  b_enc->type = TYPE_APPLICATION;
  b_enc->subtype = mutt_str_dup("pgp-signature");
  b_enc->filename = buf_strdup(sigfile);
  b_enc->use_disp = false;
  b_enc->disposition = DISP_NONE;
  b_enc->encoding = ENC_7BIT;
  b_enc->unlink = true; /* ok to remove this file after sending. */
  mutt_param_set(&b_enc->parameter, "name", "signature.asc");

cleanup:
  buf_pool_release(&sigfile);
  buf_pool_release(&signedfile);
  return rv;
}

/**
 * pgp_class_find_keys - Find the keyids of the recipients of a message - Implements CryptModuleSpecs::find_keys() - @ingroup crypto_find_keys
 */
char *pgp_class_find_keys(const struct AddressList *addrlist, bool oppenc_mode)
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
  char buf[1024] = { 0 };
  bool key_selected;
  struct AddressList hookal = TAILQ_HEAD_INITIALIZER(hookal);

  struct Address *a = NULL;
  const bool c_crypt_confirm_hook = cs_subset_bool(NeoMutt->sub, "crypt_confirm_hook");
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
        if (!oppenc_mode && c_crypt_confirm_hook)
        {
          snprintf(buf, sizeof(buf), _("Use keyID = \"%s\" for %s?"), keyid,
                   buf_string(p->mailbox));
          ans = query_yesorno_help(buf, MUTT_YES, NeoMutt->sub, "crypt_confirm_hook");
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
        snprintf(buf, sizeof(buf), _("Enter keyID for %s: "), buf_string(p->mailbox));
        k_info = pgp_ask_for_key(buf, buf_string(p->mailbox), KEYFLAG_CANENCRYPT, PGP_PUBRING);
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
 * pgp_class_encrypt_message - PGP encrypt an email - Implements CryptModuleSpecs::pgp_encrypt_message() - @ingroup crypto_pgp_encrypt_message
 *
 * @warning "b" is no longer freed in this routine, you need to free it later.
 * This is necessary for $fcc_attach.
 */
struct Body *pgp_class_encrypt_message(struct Body *b, char *keylist, bool sign,
                                       const struct AddressList *from)
{
  char buf[1024] = { 0 };
  FILE *fp_pgp_in = NULL, *fp_tmp = NULL;
  struct Body *b_enc = NULL;
  int err = 0;
  bool empty = false;
  pid_t pid;
  struct Buffer *tempfile = buf_pool_get();
  struct Buffer *pgpinfile = buf_pool_get();

  buf_mktemp(tempfile);
  FILE *fp_out = mutt_file_fopen(buf_string(tempfile), "w+");
  if (!fp_out)
  {
    mutt_perror("%s", buf_string(tempfile));
    goto cleanup;
  }

  FILE *fp_pgp_err = mutt_file_mkstemp();
  if (!fp_pgp_err)
  {
    mutt_perror(_("Can't create temporary file"));
    unlink(buf_string(tempfile));
    mutt_file_fclose(&fp_out);
    goto cleanup;
  }

  buf_mktemp(pgpinfile);
  fp_tmp = mutt_file_fopen(buf_string(pgpinfile), "w");
  if (!fp_tmp)
  {
    mutt_perror("%s", buf_string(pgpinfile));
    unlink(buf_string(tempfile));
    mutt_file_fclose(&fp_out);
    mutt_file_fclose(&fp_pgp_err);
    goto cleanup;
  }

  if (sign)
    crypt_convert_to_7bit(b);

  mutt_write_mime_header(b, fp_tmp, NeoMutt->sub);
  fputc('\n', fp_tmp);
  mutt_write_mime_body(b, fp_tmp, NeoMutt->sub);
  mutt_file_fclose(&fp_tmp);

  pid = pgp_invoke_encrypt(&fp_pgp_in, NULL, NULL, -1, fileno(fp_out),
                           fileno(fp_pgp_err), buf_string(pgpinfile), keylist, sign);
  if (pid == -1)
  {
    mutt_file_fclose(&fp_out);
    mutt_file_fclose(&fp_pgp_err);
    unlink(buf_string(pgpinfile));
    goto cleanup;
  }

  if (sign)
  {
    if (!pgp_use_gpg_agent())
      fputs(PgpPass, fp_pgp_in);
    fputc('\n', fp_pgp_in);
  }
  mutt_file_fclose(&fp_pgp_in);

  const bool c_pgp_check_exit = cs_subset_bool(NeoMutt->sub, "pgp_check_exit");
  if (filter_wait(pid) && c_pgp_check_exit)
    empty = true;

  unlink(buf_string(pgpinfile));

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
    unlink(buf_string(tempfile));
    goto cleanup;
  }

  b_enc = mutt_body_new();
  b_enc->type = TYPE_MULTIPART;
  b_enc->subtype = mutt_str_dup("encrypted");
  b_enc->encoding = ENC_7BIT;
  b_enc->use_disp = false;
  b_enc->disposition = DISP_INLINE;

  mutt_generate_boundary(&b_enc->parameter);
  mutt_param_set(&b_enc->parameter, "protocol", "application/pgp-encrypted");

  b_enc->parts = mutt_body_new();
  b_enc->parts->type = TYPE_APPLICATION;
  b_enc->parts->subtype = mutt_str_dup("pgp-encrypted");
  b_enc->parts->encoding = ENC_7BIT;

  b_enc->parts->next = mutt_body_new();
  b_enc->parts->next->type = TYPE_APPLICATION;
  b_enc->parts->next->subtype = mutt_str_dup("octet-stream");
  b_enc->parts->next->encoding = ENC_7BIT;
  b_enc->parts->next->filename = buf_strdup(tempfile);
  b_enc->parts->next->use_disp = true;
  b_enc->parts->next->disposition = DISP_ATTACH;
  b_enc->parts->next->unlink = true; /* delete after sending the message */
  b_enc->parts->next->d_filename = mutt_str_dup("msg.asc"); /* non pgp/mime can save */

cleanup:
  buf_pool_release(&tempfile);
  buf_pool_release(&pgpinfile);
  return b_enc;
}

/**
 * pgp_class_traditional_encryptsign - Create an inline PGP encrypted, signed email - Implements CryptModuleSpecs::pgp_traditional_encryptsign() - @ingroup crypto_pgp_traditional_encryptsign
 */
struct Body *pgp_class_traditional_encryptsign(struct Body *b, SecurityFlags flags, char *keylist)
{
  struct Body *b_enc = NULL;
  char body_charset[256] = { 0 };
  const char *from_charset = NULL;
  const char *send_charset = NULL;
  bool empty = false;
  bool err;
  char buf[256] = { 0 };
  pid_t pid;
  struct Buffer *pgpinfile = buf_pool_get();
  struct Buffer *pgpoutfile = buf_pool_get();

  if (b->type != TYPE_TEXT)
    goto cleanup;
  if (!mutt_istr_equal(b->subtype, "plain"))
    goto cleanup;

  FILE *fp_body = fopen(b->filename, "r");
  if (!fp_body)
  {
    mutt_perror("%s", b->filename);
    goto cleanup;
  }

  buf_mktemp(pgpinfile);
  FILE *fp_pgp_in = mutt_file_fopen(buf_string(pgpinfile), "w");
  if (!fp_pgp_in)
  {
    mutt_perror("%s", buf_string(pgpinfile));
    mutt_file_fclose(&fp_body);
    goto cleanup;
  }

  /* The following code is really correct:  If noconv is set,
   * b's charset parameter contains the on-disk character set, and
   * we have to convert from that to utf-8.  If noconv is not set,
   * we have to convert from $charset to utf-8.  */

  mutt_body_get_charset(b, body_charset, sizeof(body_charset));
  if (b->noconv)
    from_charset = body_charset;
  else
    from_charset = cc_charset();

  if (mutt_ch_is_us_ascii(body_charset))
  {
    send_charset = "us-ascii";
    mutt_file_copy_stream(fp_body, fp_pgp_in);
  }
  else
  {
    int c;
    struct FgetConv *fc = NULL;

    if (flags & SEC_ENCRYPT)
      send_charset = "us-ascii";
    else
      send_charset = "utf-8";

    /* fromcode is assumed to be correct: we set flags to 0 */
    fc = mutt_ch_fgetconv_open(fp_body, from_charset, "utf-8", MUTT_ICONV_NO_FLAGS);
    while ((c = mutt_ch_fgetconv(fc)) != EOF)
      fputc(c, fp_pgp_in);

    mutt_ch_fgetconv_close(&fc);
  }
  mutt_file_fclose(&fp_body);
  mutt_file_fclose(&fp_pgp_in);

  buf_mktemp(pgpoutfile);
  FILE *fp_pgp_out = mutt_file_fopen(buf_string(pgpoutfile), "w+");
  FILE *fp_pgp_err = mutt_file_mkstemp();
  if (!fp_pgp_out || !fp_pgp_err)
  {
    mutt_perror("%s", fp_pgp_out ? "Can't create temporary file" : buf_string(pgpoutfile));
    unlink(buf_string(pgpinfile));
    if (fp_pgp_out)
    {
      mutt_file_fclose(&fp_pgp_out);
      unlink(buf_string(pgpoutfile));
    }
    mutt_file_fclose(&fp_pgp_err);
    goto cleanup;
  }

  pid = pgp_invoke_traditional(&fp_pgp_in, NULL, NULL, -1, fileno(fp_pgp_out),
                               fileno(fp_pgp_err), buf_string(pgpinfile), keylist, flags);
  if (pid == -1)
  {
    mutt_perror(_("Can't invoke PGP"));
    mutt_file_fclose(&fp_pgp_out);
    mutt_file_fclose(&fp_pgp_err);
    mutt_file_unlink(buf_string(pgpinfile));
    unlink(buf_string(pgpoutfile));
    goto cleanup;
  }

  if (pgp_use_gpg_agent())
    *PgpPass = '\0';
  if (flags & SEC_SIGN)
    fprintf(fp_pgp_in, "%s\n", PgpPass);
  mutt_file_fclose(&fp_pgp_in);

  const bool c_pgp_check_exit = cs_subset_bool(NeoMutt->sub, "pgp_check_exit");
  if (filter_wait(pid) && c_pgp_check_exit)
    empty = true;

  mutt_file_unlink(buf_string(pgpinfile));

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
    unlink(buf_string(pgpoutfile));
    goto cleanup;
  }

  b_enc = mutt_body_new();

  b_enc->encoding = ENC_7BIT;

  b_enc->type = TYPE_TEXT;
  b_enc->subtype = mutt_str_dup("plain");

  mutt_param_set(&b_enc->parameter, "x-action",
                 (flags & SEC_ENCRYPT) ? "pgp-encrypted" : "pgp-signed");
  mutt_param_set(&b_enc->parameter, "charset", send_charset);

  b_enc->filename = buf_strdup(pgpoutfile);

  b_enc->disposition = DISP_NONE;
  b_enc->unlink = true;

  b_enc->noconv = true;
  b_enc->use_disp = false;

  if (!(flags & SEC_ENCRYPT))
    b_enc->encoding = b->encoding;

cleanup:
  buf_pool_release(&pgpinfile);
  buf_pool_release(&pgpoutfile);
  return b_enc;
}

/**
 * pgp_class_send_menu - Ask the user whether to sign and/or encrypt the email - Implements CryptModuleSpecs::send_menu() - @ingroup crypto_send_menu
 */
SecurityFlags pgp_class_send_menu(struct Email *e)
{
  struct PgpKeyInfo *p = NULL;
  const char *prompt = NULL;
  const char *letters = NULL;
  const char *choices = NULL;
  char promptbuf[1024] = { 0 };
  int choice;

  if (!(WithCrypto & APPLICATION_PGP))
    return e->security;

  /* If autoinline and no crypto options set, then set inline. */
  const bool c_pgp_auto_inline = cs_subset_bool(NeoMutt->sub, "pgp_auto_inline");
  if (c_pgp_auto_inline &&
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
  const bool c_crypt_opportunistic_encrypt = cs_subset_bool(NeoMutt->sub, "crypt_opportunistic_encrypt");
  if (c_crypt_opportunistic_encrypt && (e->security & SEC_OPPENCRYPT))
  {
    if (e->security & (SEC_ENCRYPT | SEC_SIGN))
    {
      snprintf(promptbuf, sizeof(promptbuf),
               /* L10N: PGP options (inline) (opportunistic encryption is on) */
               _("PGP (s)ign, sign (a)s, %s format, (c)lear, or (o)ppenc mode off?"),
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
  else if (c_crypt_opportunistic_encrypt)
  {
    /* Opportunistic encryption option is set, but is toggled off
     * for this message.  */
    /* When the message is not selected for signing or encryption, the toggle
     * between PGP/MIME and Traditional doesn't make sense.  */
    if (e->security & (SEC_ENCRYPT | SEC_SIGN))
    {
      snprintf(promptbuf, sizeof(promptbuf),
               /* L10N: PGP options (inline) (opportunistic encryption is off) */
               _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, %s format, (c)lear, or (o)ppenc mode?"),
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
      prompt = _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (c)lear, or (o)ppenc mode?");
      /* L10N: PGP options (opportunistic encryption is off) */
      letters = _("esabco");
      choices = "esabcO";
    }
  }
  else
  {
    /* Opportunistic encryption is unset */
    if (e->security & (SEC_ENCRYPT | SEC_SIGN))
    {
      snprintf(promptbuf, sizeof(promptbuf),
               /* L10N: PGP options (inline) */
               _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, %s format, or (c)lear?"),
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

  choice = mw_multi_choice(prompt, letters);
  if (choice > 0)
  {
    switch (choices[choice - 1])
    {
      case 'a': /* sign (a)s */
        OptPgpCheckTrust = false;

        p = pgp_ask_for_key(_("Sign as: "), NULL, KEYFLAG_NO_FLAGS, PGP_SECRING);
        if (p)
        {
          char input_signas[128] = { 0 };
          snprintf(input_signas, sizeof(input_signas), "0x%s", pgp_fpr_or_lkeyid(p));
          cs_subset_str_string_set(NeoMutt->sub, "pgp_sign_as", input_signas, NULL);
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
