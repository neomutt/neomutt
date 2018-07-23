/**
 * @file
 * PGP sign, encrypt, check routines
 *
 * @authors
 * Copyright (C) 1996-1997,2000,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1998-2005 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2004 g10 Code GmbH
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
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "email/email.h"
#include "mutt.h"
#include "pgp.h"
#include "crypt.h"
#include "cryptglue.h"
#include "curs_lib.h"
#include "filter.h"
#include "globals.h"
#include "handler.h"
#include "hook.h"
#include "mutt_attach.h"
#include "muttlib.h"
#include "ncrypt.h"
#include "options.h"
#include "pgpinvoke.h"
#include "pgpkey.h"
#include "pgplib.h"
#include "pgpmicalg.h"
#include "sendlib.h"
#include "state.h"

/* These Config Variables are only used in ncrypt/pgp.c */
bool PgpCheckExit;
bool PgpCheckGpgDecryptStatusFd;
struct Regex *PgpDecryptionOkay;
struct Regex *PgpGoodSign;
long PgpTimeout;
bool PgpUseGpgAgent;

char PgpPass[LONG_STRING];
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
int pgp_class_valid_passphrase(void)
{
  time_t now = time(NULL);

  if (pgp_use_gpg_agent())
  {
    *PgpPass = 0;
    return 1; /* handled by gpg-agent */
  }

  if (now < PgpExptime)
  {
    /* Use cached copy.  */
    return 1;
  }

  pgp_class_void_passphrase();

  if (mutt_get_password(_("Enter PGP passphrase:"), PgpPass, sizeof(PgpPass)) == 0)
  {
    PgpExptime = mutt_date_add_timeout(time(NULL), PgpTimeout);
    return 1;
  }
  else
    PgpExptime = 0;

  return 0;
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
  if (!PgpUseGpgAgent)
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
  if ((k->flags & KEYFLAG_SUBKEY) && k->parent && PgpIgnoreSubkeys)
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
  if (PgpLongIds)
    return k->keyid;
  else
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
 * @param fpin  File to read from
 * @param fpout File to write to
 * @retval  0 Success
 * @retval -1 Error
 */
static int pgp_copy_checksig(FILE *fpin, FILE *fpout)
{
  if (!fpin || !fpout)
    return -1;

  int rc = -1;

  if (PgpGoodSign && PgpGoodSign->regex)
  {
    char *line = NULL;
    int lineno = 0;
    size_t linelen;

    while ((line = mutt_file_read_line(line, &linelen, fpin, &lineno, 0)) != NULL)
    {
      if (regexec(PgpGoodSign->regex, line, 0, NULL, 0) == 0)
      {
        mutt_debug(2, "\"%s\" matches regex.\n", line);
        rc = 0;
      }
      else
        mutt_debug(2, "\"%s\" doesn't match regex.\n", line);

      if (strncmp(line, "[GNUPG:] ", 9) == 0)
        continue;
      fputs(line, fpout);
      fputc('\n', fpout);
    }
    FREE(&line);
  }
  else
  {
    mutt_debug(2, "No pattern.\n");
    mutt_file_copy_stream(fpin, fpout);
    rc = 1;
  }

  return rc;
}

/**
 * pgp_check_pgp_decryption_okay_regex - Check PGP output to look for successful outcome
 * @param fpin File to read from
 * @retval  0 Success
 * @retval -1 Error
 *
 * Checks PGP output messages to look for the $pgp_decryption_okay message.
 * This protects against messages with multipart/encrypted headers but which
 * aren't actually encrypted.  See ticket #3770
 */
static int pgp_check_pgp_decryption_okay_regex(FILE *fpin)
{
  int rc = -1;

  if (PgpDecryptionOkay && PgpDecryptionOkay->regex)
  {
    char *line = NULL;
    int lineno = 0;
    size_t linelen;

    while ((line = mutt_file_read_line(line, &linelen, fpin, &lineno, 0)) != NULL)
    {
      if (regexec(PgpDecryptionOkay->regex, line, 0, NULL, 0) == 0)
      {
        mutt_debug(2, "\"%s\" matches regex.\n", line);
        rc = 0;
        break;
      }
      else
        mutt_debug(2, "\"%s\" doesn't match regex.\n", line);
    }
    FREE(&line);
  }
  else
  {
    mutt_debug(2, "No pattern.\n");
    rc = 1;
  }

  return rc;
}

/**
 * pgp_check_decryption_okay - Check GPG output for status codes
 * @param fpin File to read from
 * @retval  1 - no patterns were matched (if delegated to decryption_okay_regexp)
 * @retval  0 - DECRYPTION_OKAY was seen, with no PLAINTEXT outside.
 * @retval -1 - No decryption status codes were encountered
 * @retval -2 - PLAINTEXT was encountered outside of DECRYPTION delimeters.
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
static int pgp_check_decryption_okay(FILE *fpin)
{
  int rv = -1;
  char *line = NULL, *s = NULL;
  int lineno = 0;
  size_t linelen;
  int inside_decrypt = 0;

  if (!PgpCheckGpgDecryptStatusFd)
    return pgp_check_pgp_decryption_okay_regex(fpin);

  while ((line = mutt_file_read_line(line, &linelen, fpin, &lineno, 0)) != NULL)
  {
    if (mutt_str_strncmp(line, "[GNUPG:] ", 9) != 0)
      continue;
    s = line + 9;
    mutt_debug(2, "checking \"%s\".\n", line);
    if (mutt_str_strncmp(s, "BEGIN_DECRYPTION", 16) == 0)
      inside_decrypt = 1;
    else if (mutt_str_strncmp(s, "END_DECRYPTION", 14) == 0)
      inside_decrypt = 0;
    else if (mutt_str_strncmp(s, "PLAINTEXT", 9) == 0)
    {
      if (!inside_decrypt)
      {
        mutt_debug(2, "\tPLAINTEXT encountered outside of DECRYPTION.\n");
        if (rv > -2)
          rv = -2;
        break;
      }
    }
    else if (mutt_str_strncmp(s, "DECRYPTION_FAILED", 17) == 0)
    {
      mutt_debug(2, "\tDECRYPTION_FAILED encountered.  Failure.\n");
      rv = -3;
      break;
    }
    else if (mutt_str_strncmp(s, "DECRYPTION_OKAY", 15) == 0)
    {
      /* Don't break out because we still have to check for
       * PLAINTEXT outside of the decryption boundaries. */
      mutt_debug(2, "\tDECRYPTION_OKAY encountered.\n");
      if (rv > -2)
        rv = 0;
    }
  }
  FREE(&line);

  return rv;
}

/**
 * pgp_copy_clearsigned - Copy a clearsigned message, stripping the signature
 * @param fpin    File to read from
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
static void pgp_copy_clearsigned(FILE *fpin, struct State *s, char *charset)
{
  char buf[HUGE_STRING];
  bool complete, armor_header;

  rewind(fpin);

  /* fromcode comes from the MIME Content-Type charset label. It might
   * be a wrong label, so we want the ability to do corrections via
   * charset-hooks. Therefore we set flags to MUTT_ICONV_HOOK_FROM.
   */
  struct FgetConv *fc = mutt_ch_fgetconv_open(fpin, charset, Charset, MUTT_ICONV_HOOK_FROM);

  for (complete = true, armor_header = true; mutt_ch_fgetconvs(buf, sizeof(buf), fc) != NULL;
       complete = (strchr(buf, '\n') != NULL))
  {
    if (!complete)
    {
      if (!armor_header)
        state_puts(buf, s);
      continue;
    }

    if (mutt_str_strcmp(buf, "-----BEGIN PGP SIGNATURE-----\n") == 0)
      break;

    if (armor_header)
    {
      char *p = mutt_str_skip_whitespace(buf);
      if (*p == '\0')
        armor_header = false;
      continue;
    }

    if (s->prefix)
      state_puts(s->prefix, s);

    if (buf[0] == '-' && buf[1] == ' ')
      state_puts(buf + 2, s);
    else
      state_puts(buf, s);
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
  int rv, rc;
  int c = 1;
  long bytes;
  LOFF_T last_pos, offset;
  char buf[HUGE_STRING];
  char tmpfname[PATH_MAX];
  FILE *pgpout = NULL, *pgpin = NULL, *pgperr = NULL;
  FILE *tmpfp = NULL;
  pid_t thepid;

  bool maybe_goodsig = true;
  bool have_any_sigs = false;

  char *gpgcharset = NULL;
  char body_charset[STRING];
  mutt_body_get_charset(m, body_charset, sizeof(body_charset));

  rc = 0;

  fseeko(s->fpin, m->offset, SEEK_SET);
  last_pos = m->offset;

  for (bytes = m->length; bytes > 0;)
  {
    if (fgets(buf, sizeof(buf), s->fpin) == NULL)
      break;

    offset = ftello(s->fpin);
    bytes -= (offset - last_pos); /* don't rely on mutt_str_strlen(buf) */
    last_pos = offset;

    if (mutt_str_strncmp("-----BEGIN PGP ", buf, 15) == 0)
    {
      clearsign = false;
      could_not_decrypt = false;
      decrypt_okay_rc = 0;

      if (mutt_str_strcmp("MESSAGE-----\n", buf + 15) == 0)
        needpass = 1;
      else if (mutt_str_strcmp("SIGNED MESSAGE-----\n", buf + 15) == 0)
      {
        clearsign = true;
        needpass = 0;
      }
      else if (mutt_str_strcmp("PUBLIC KEY BLOCK-----\n", buf + 15) == 0)
      {
        needpass = 0;
        pgp_keyblock = true;
      }
      else
      {
        /* XXX we may wish to recode here */
        if (s->prefix)
          state_puts(s->prefix, s);
        state_puts(buf, s);
        continue;
      }

      have_any_sigs = have_any_sigs || (clearsign && (s->flags & MUTT_VERIFY));

      /* Copy PGP material to temporary file */
      mutt_mktemp(tmpfname, sizeof(tmpfname));
      tmpfp = mutt_file_fopen(tmpfname, "w+");
      if (!tmpfp)
      {
        mutt_perror(tmpfname);
        FREE(&gpgcharset);
        return -1;
      }

      fputs(buf, tmpfp);
      while (bytes > 0 && fgets(buf, sizeof(buf) - 1, s->fpin) != NULL)
      {
        offset = ftello(s->fpin);
        bytes -= (offset - last_pos); /* don't rely on mutt_str_strlen(buf) */
        last_pos = offset;

        fputs(buf, tmpfp);

        if ((needpass && (mutt_str_strcmp("-----END PGP MESSAGE-----\n", buf) == 0)) ||
            (!needpass &&
             ((mutt_str_strcmp("-----END PGP SIGNATURE-----\n", buf) == 0) ||
              (mutt_str_strcmp("-----END PGP PUBLIC KEY BLOCK-----\n", buf) == 0))))
        {
          break;
        }
        /* remember optional Charset: armor header as defined by RFC4880 */
        if (mutt_str_strncmp("Charset: ", buf, 9) == 0)
        {
          size_t l = 0;
          FREE(&gpgcharset);
          gpgcharset = mutt_str_strdup(buf + 9);
          l = mutt_str_strlen(gpgcharset);
          if ((l > 0) && (gpgcharset[l - 1] == '\n'))
            gpgcharset[l - 1] = 0;
          if (!mutt_ch_check_charset(gpgcharset, 0))
            mutt_str_replace(&gpgcharset, "UTF-8");
        }
      }

      /* leave tmpfp open in case we still need it - but flush it! */
      fflush(tmpfp);

      /* Invoke PGP if needed */
      if (!clearsign || (s->flags & MUTT_VERIFY))
      {
        pgpout = mutt_file_mkstemp();
        if (!pgpout)
        {
          mutt_perror(_("Can't create temporary file"));
          rc = -1;
          goto out;
        }

        pgperr = mutt_file_mkstemp();
        if (!pgperr)
        {
          mutt_perror(_("Can't create temporary file"));
          rc = -1;
          goto out;
        }

        thepid = pgp_invoke_decode(&pgpin, NULL, NULL, -1, fileno(pgpout),
                                   fileno(pgperr), tmpfname, (needpass != 0));
        if (thepid == -1)
        {
          mutt_file_fclose(&pgpout);
          maybe_goodsig = false;
          pgpin = NULL;
          state_attach_puts(
              _("[-- Error: unable to create PGP subprocess! --]\n"), s);
        }
        else /* PGP started successfully */
        {
          if (needpass)
          {
            if (!pgp_class_valid_passphrase())
              pgp_class_void_passphrase();
            if (pgp_use_gpg_agent())
              *PgpPass = 0;
            fprintf(pgpin, "%s\n", PgpPass);
          }

          mutt_file_fclose(&pgpin);

          rv = mutt_wait_filter(thepid);

          fflush(pgperr);
          /* If we are expecting an encrypted message, verify status fd output.
           * Note that BEGIN PGP MESSAGE does not guarantee the content is encrypted,
           * so we need to be more selective about the value of decrypt_okay_rc.
           *
           * -3 indicates we actively found a DECRYPTION_FAILED.
           * -2 and -1 indicate part or all of the content was plaintext.
           */
          if (needpass)
          {
            rewind(pgperr);
            decrypt_okay_rc = pgp_check_decryption_okay(pgperr);
            if (decrypt_okay_rc <= -3)
              mutt_file_fclose(&pgpout);
          }

          if (s->flags & MUTT_DISPLAY)
          {
            rewind(pgperr);
            crypt_current_time(s, "PGP");
            rc = pgp_copy_checksig(pgperr, s->fpout);

            if (rc == 0)
              have_any_sigs = true;
            /* Sig is bad if
             * gpg_good_sign-pattern did not match || pgp_decode_command returned not 0
             * Sig _is_ correct if
             *  gpg_good_sign="" && pgp_decode_command returned 0
             */
            if (rc == -1 || rv)
              maybe_goodsig = false;

            state_attach_puts(_("[-- End of PGP output --]\n\n"), s);
          }
          if (pgp_use_gpg_agent())
            mutt_need_hard_redraw();
        }

        /* treat empty result as sign of failure */
        /* TODO: maybe on failure neomutt should include the original undecoded text. */
        if (pgpout)
        {
          rewind(pgpout);
          c = fgetc(pgpout);
          ungetc(c, pgpout);
        }
        if (!clearsign && (!pgpout || c == EOF))
        {
          could_not_decrypt = true;
          pgp_class_void_passphrase();
        }

        if ((could_not_decrypt || (decrypt_okay_rc <= -3)) && !(s->flags & MUTT_DISPLAY))
        {
          mutt_error(_("Could not decrypt PGP message"));
          rc = -1;
          goto out;
        }
      }

      /* Now, copy cleartext to the screen.  */
      if (s->flags & MUTT_DISPLAY)
      {
        if (needpass)
          state_attach_puts(_("[-- BEGIN PGP MESSAGE --]\n\n"), s);
        else if (pgp_keyblock)
          state_attach_puts(_("[-- BEGIN PGP PUBLIC KEY BLOCK --]\n"), s);
        else
          state_attach_puts(_("[-- BEGIN PGP SIGNED MESSAGE --]\n\n"), s);
      }

      if (clearsign)
      {
        rewind(tmpfp);
        if (tmpfp)
          pgp_copy_clearsigned(tmpfp, s, body_charset);
      }
      else if (pgpout)
      {
        struct FgetConv *fc = NULL;
        int ch;
        char *expected_charset = gpgcharset && *gpgcharset ? gpgcharset : "utf-8";

        mutt_debug(4, "pgp: recoding inline from [%s] to [%s]\n", expected_charset, Charset);

        rewind(pgpout);
        state_set_prefix(s);
        fc = mutt_ch_fgetconv_open(pgpout, expected_charset, Charset, MUTT_ICONV_HOOK_FROM);
        while ((ch = mutt_ch_fgetconv(fc)) != EOF)
          state_prefix_putc(ch, s);
        mutt_ch_fgetconv_close(&fc);
      }

      /* Multiple PGP blocks can exist, so these need to be closed and
       * unlinked inside the loop.
       */
      mutt_file_fclose(&tmpfp);
      mutt_file_unlink(tmpfname);
      mutt_file_fclose(&pgpout);
      mutt_file_fclose(&pgperr);

      if (s->flags & MUTT_DISPLAY)
      {
        state_putc('\n', s);
        if (needpass)
        {
          state_attach_puts(_("[-- END PGP MESSAGE --]\n"), s);
          if (could_not_decrypt || (decrypt_okay_rc <= -3))
            mutt_error(_("Could not decrypt PGP message"));
          else if (decrypt_okay_rc < 0)
            mutt_error(_("PGP message was not encrypted."));
          else
            mutt_message(_("PGP message successfully decrypted."));
        }
        else if (pgp_keyblock)
          state_attach_puts(_("[-- END PGP PUBLIC KEY BLOCK --]\n"), s);
        else
          state_attach_puts(_("[-- END PGP SIGNED MESSAGE --]\n"), s);
      }
    }
    else
    {
      /* A traditional PGP part may mix signed and unsigned content */
      /* XXX we may wish to recode here */
      if (s->prefix)
        state_puts(s->prefix, s);
      state_puts(buf, s);
    }
  }

  rc = 0;

out:
  m->goodsig = (maybe_goodsig && have_any_sigs);

  if (tmpfp)
  {
    mutt_file_fclose(&tmpfp);
    mutt_file_unlink(tmpfname);
  }
  mutt_file_fclose(&pgpout);
  mutt_file_fclose(&pgperr);

  FREE(&gpgcharset);

  if (needpass == -1)
  {
    state_attach_puts(
        _("[-- Error: could not find beginning of PGP message! --]\n\n"), s);
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
  char tempfile[PATH_MAX];
  char buf[HUGE_STRING];
  FILE *tfp = NULL;

  short sgn = 0;
  short enc = 0;
  short key = 0;

  if (b->type != TYPE_TEXT)
    return 0;

  mutt_mktemp(tempfile, sizeof(tempfile));
  if (mutt_decode_save_attachment(fp, b, tempfile, 0, 0) != 0)
  {
    unlink(tempfile);
    return 0;
  }

  tfp = fopen(tempfile, "r");
  if (!tfp)
  {
    unlink(tempfile);
    return 0;
  }

  while (fgets(buf, sizeof(buf), tfp))
  {
    if (mutt_str_strncmp("-----BEGIN PGP ", buf, 15) == 0)
    {
      if (mutt_str_strcmp("MESSAGE-----\n", buf + 15) == 0)
        enc = 1;
      else if (mutt_str_strcmp("SIGNED MESSAGE-----\n", buf + 15) == 0)
        sgn = 1;
      else if (mutt_str_strcmp("PUBLIC KEY BLOCK-----\n", buf + 15) == 0)
        key = 1;
    }
  }
  mutt_file_fclose(&tfp);
  unlink(tempfile);

  if (!enc && !sgn && !key)
    return 0;

  /* fix the content type */

  mutt_param_set(&b->parameter, "format", "fixed");
  if (enc)
    mutt_param_set(&b->parameter, "x-action", "pgp-encrypted");
  else if (sgn)
    mutt_param_set(&b->parameter, "x-action", "pgp-signed");
  else if (key)
    mutt_param_set(&b->parameter, "x-action", "pgp-keys");

  return 1;
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
  char sigfile[PATH_MAX];
  FILE *pgpout = NULL;
  pid_t thepid;
  int badsig = -1;

  snprintf(sigfile, sizeof(sigfile), "%s.asc", tempfile);

  FILE *fp = mutt_file_fopen(sigfile, "w");
  if (!fp)
  {
    mutt_perror(sigfile);
    return -1;
  }

  fseeko(s->fpin, sigbdy->offset, SEEK_SET);
  mutt_file_copy_bytes(s->fpin, fp, sigbdy->length);
  mutt_file_fclose(&fp);

  FILE *pgperr = mutt_file_mkstemp();
  if (!pgperr)
  {
    mutt_perror(_("Can't create temporary file"));
    unlink(sigfile);
    return -1;
  }

  crypt_current_time(s, "PGP");

  thepid = pgp_invoke_verify(NULL, &pgpout, NULL, -1, -1, fileno(pgperr), tempfile, sigfile);
  if (thepid != -1)
  {
    if (pgp_copy_checksig(pgpout, s->fpout) >= 0)
      badsig = 0;

    mutt_file_fclose(&pgpout);
    fflush(pgperr);
    rewind(pgperr);

    if (pgp_copy_checksig(pgperr, s->fpout) >= 0)
      badsig = 0;

    const int rv = mutt_wait_filter(thepid);
    if (rv)
      badsig = -1;

    mutt_debug(1, "mutt_wait_filter returned %d.\n", rv);
  }

  mutt_file_fclose(&pgperr);

  state_attach_puts(_("[-- End of PGP output --]\n\n"), s);

  mutt_file_unlink(sigfile);

  mutt_debug(1, "returning %d.\n", badsig);

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
  char tempfname[PATH_MAX];

  mutt_mktemp(tempfname, sizeof(tempfname));
  FILE *tempfp = mutt_file_fopen(tempfname, "w");
  if (!tempfp)
  {
    mutt_perror(tempfname);
    return;
  }

  s.fpin = fp;
  s.fpout = tempfp;

  mutt_body_handler(top, &s);

  mutt_file_fclose(&tempfp);

  pgp_class_invoke_import(tempfname);
  mutt_any_key_to_continue(NULL);

  mutt_file_unlink(tempfname);
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
 * @param a     Body of attachment
 * @param s     State to use
 * @param fpout File to write to
 * @param p     Body of parent (main email)
 * @retval ptr New Body for the attachment
 */
static struct Body *pgp_decrypt_part(struct Body *a, struct State *s,
                                     FILE *fpout, struct Body *p)
{
  if (!a || !s || !fpout || !p)
    return NULL;

  char buf[LONG_STRING];
  FILE *pgpin = NULL, *pgpout = NULL, *pgptmp = NULL;
  struct stat info;
  struct Body *tattach = NULL;
  char pgptmpfile[PATH_MAX];
  pid_t thepid;
  int rv;

  FILE *pgperr = mutt_file_mkstemp();
  if (!pgperr)
  {
    mutt_perror(_("Can't create temporary file"));
    return NULL;
  }

  mutt_mktemp(pgptmpfile, sizeof(pgptmpfile));
  pgptmp = mutt_file_fopen(pgptmpfile, "w");
  if (!pgptmp)
  {
    mutt_perror(pgptmpfile);
    mutt_file_fclose(&pgperr);
    return NULL;
  }

  /* Position the stream at the beginning of the body, and send the data to
   * the temporary file.
   */

  fseeko(s->fpin, a->offset, SEEK_SET);
  mutt_file_copy_bytes(s->fpin, pgptmp, a->length);
  mutt_file_fclose(&pgptmp);

  thepid = pgp_invoke_decrypt(&pgpin, &pgpout, NULL, -1, -1, fileno(pgperr), pgptmpfile);
  if (thepid == -1)
  {
    mutt_file_fclose(&pgperr);
    unlink(pgptmpfile);
    if (s->flags & MUTT_DISPLAY)
    {
      state_attach_puts(
          _("[-- Error: could not create a PGP subprocess! --]\n\n"), s);
    }
    return NULL;
  }

  /* send the PGP passphrase to the subprocess.  Never do this if the
     agent is active, because this might lead to a passphrase send as
     the message. */
  if (!pgp_use_gpg_agent())
    fputs(PgpPass, pgpin);
  fputc('\n', pgpin);
  mutt_file_fclose(&pgpin);

  /* Read the output from PGP, and make sure to change CRLF to LF, otherwise
   * read_mime_header has a hard time parsing the message.
   */
  while (fgets(buf, sizeof(buf) - 1, pgpout) != NULL)
  {
    size_t len = mutt_str_strlen(buf);
    if (len > 1 && buf[len - 2] == '\r')
      strcpy(buf + len - 2, "\n");
    fputs(buf, fpout);
  }

  mutt_file_fclose(&pgpout);
  rv = mutt_wait_filter(thepid);
  mutt_file_unlink(pgptmpfile);

  fflush(pgperr);
  rewind(pgperr);
  if (pgp_check_decryption_okay(pgperr) < 0)
  {
    mutt_error(_("Decryption failed"));
    pgp_class_void_passphrase();
    mutt_file_fclose(&pgperr);
    return NULL;
  }

  if (s->flags & MUTT_DISPLAY)
  {
    rewind(pgperr);
    if (pgp_copy_checksig(pgperr, s->fpout) == 0 && !rv)
      p->goodsig = true;
    else
      p->goodsig = false;
    state_attach_puts(_("[-- End of PGP output --]\n\n"), s);
  }
  mutt_file_fclose(&pgperr);

  fflush(fpout);
  rewind(fpout);

  if (pgp_use_gpg_agent())
    mutt_need_hard_redraw();

  if (fgetc(fpout) == EOF)
  {
    mutt_error(_("Decryption failed"));
    pgp_class_void_passphrase();
    return NULL;
  }

  rewind(fpout);

  tattach = mutt_read_mime_header(fpout, 0);
  if (tattach)
  {
    /* Need to set the length of this body part.  */
    fstat(fileno(fpout), &info);
    tattach->length = info.st_size - tattach->offset;

    /* See if we need to recurse on this MIME part.  */
    mutt_parse_part(fpout, tattach);
  }

  return tattach;
}

/**
 * pgp_class_decrypt_mime - Implements CryptModuleSpecs::decrypt_mime()
 */
int pgp_class_decrypt_mime(FILE *fpin, FILE **fpout, struct Body *b, struct Body **cur)
{
  struct State s = { 0 };
  struct Body *p = b;
  bool need_decode = false;
  int saved_type = 0;
  LOFF_T saved_offset = 0;
  size_t saved_length = 0;
  FILE *decoded_fp = NULL;
  int rc = 0;

  if (mutt_is_valid_multipart_pgp_encrypted(b))
    b = b->parts->next;
  else if (mutt_is_malformed_multipart_pgp_encrypted(b))
  {
    b = b->parts->next->next;
    need_decode = true;
  }
  else
    return -1;

  s.fpin = fpin;

  if (need_decode)
  {
    saved_type = b->type;
    saved_offset = b->offset;
    saved_length = b->length;

    decoded_fp = mutt_file_mkstemp();
    if (!decoded_fp)
    {
      mutt_perror(_("Can't create temporary file"));
      return -1;
    }

    fseeko(s.fpin, b->offset, SEEK_SET);
    s.fpout = decoded_fp;

    mutt_decode_attachment(b, &s);

    fflush(decoded_fp);
    b->length = ftello(decoded_fp);
    b->offset = 0;
    rewind(decoded_fp);
    s.fpin = decoded_fp;
    s.fpout = 0;
  }

  *fpout = mutt_file_mkstemp();
  if (!*fpout)
  {
    mutt_perror(_("Can't create temporary file"));
    rc = -1;
    goto bail;
  }

  *cur = pgp_decrypt_part(b, &s, *fpout, p);
  if (!*cur)
    rc = -1;
  rewind(*fpout);

bail:
  if (need_decode)
  {
    b->type = saved_type;
    b->length = saved_length;
    b->offset = saved_offset;
    mutt_file_fclose(&decoded_fp);
  }

  return rc;
}

/**
 * pgp_class_encrypted_handler - Implements CryptModuleSpecs::encrypted_handler()
 */
int pgp_class_encrypted_handler(struct Body *a, struct State *s)
{
  FILE *fpin = NULL;
  struct Body *tattach = NULL;
  int rc = 0;

  FILE *fpout = mutt_file_mkstemp();
  if (!fpout)
  {
    mutt_perror(_("Can't create temporary file"));
    if (s->flags & MUTT_DISPLAY)
      state_attach_puts(_("[-- Error: could not create temporary file! --]\n"), s);
    return -1;
  }

  if (s->flags & MUTT_DISPLAY)
    crypt_current_time(s, "PGP");

  tattach = pgp_decrypt_part(a, s, fpout, a);
  if (tattach)
  {
    if (s->flags & MUTT_DISPLAY)
    {
      state_attach_puts(
          _("[-- The following data is PGP/MIME encrypted --]\n\n"), s);
    }

    fpin = s->fpin;
    s->fpin = fpout;
    rc = mutt_body_handler(tattach, s);
    s->fpin = fpin;

    /* if a multipart/signed is the _only_ sub-part of a
     * multipart/encrypted, cache signature verification
     * status.
     */
    if (mutt_is_multipart_signed(tattach) && !tattach->next)
      a->goodsig |= tattach->goodsig;

    if (s->flags & MUTT_DISPLAY)
    {
      state_puts("\n", s);
      state_attach_puts(_("[-- End of PGP/MIME encrypted data --]\n"), s);
    }

    mutt_body_free(&tattach);
    /* clear 'Invoking...' message, since there's no error */
    mutt_message(_("PGP message successfully decrypted."));
  }
  else
  {
    mutt_error(_("Could not decrypt PGP message"));
    /* void the passphrase, even if it's not necessarily the problem */
    pgp_class_void_passphrase();
    rc = -1;
  }

  mutt_file_fclose(&fpout);

  return rc;
}

/* ----------------------------------------------------------------------------
 * Routines for sending PGP/MIME messages.
 */

/**
 * pgp_class_sign_message - Implements CryptModuleSpecs::sign_message()
 */
struct Body *pgp_class_sign_message(struct Body *a)
{
  struct Body *t = NULL;
  char buffer[LONG_STRING];
  char sigfile[PATH_MAX], signedfile[PATH_MAX];
  FILE *pgpin = NULL, *pgpout = NULL, *pgperr = NULL, *sfp = NULL;
  bool err = false;
  bool empty = true;
  pid_t thepid;

  crypt_convert_to_7bit(a); /* Signed data _must_ be in 7-bit format. */

  mutt_mktemp(sigfile, sizeof(sigfile));
  FILE *fp = mutt_file_fopen(sigfile, "w");
  if (!fp)
  {
    return NULL;
  }

  mutt_mktemp(signedfile, sizeof(signedfile));
  sfp = mutt_file_fopen(signedfile, "w");
  if (!sfp)
  {
    mutt_perror(signedfile);
    mutt_file_fclose(&fp);
    unlink(sigfile);
    return NULL;
  }

  mutt_write_mime_header(a, sfp);
  fputc('\n', sfp);
  mutt_write_mime_body(a, sfp);
  mutt_file_fclose(&sfp);

  thepid = pgp_invoke_sign(&pgpin, &pgpout, &pgperr, -1, -1, -1, signedfile);
  if (thepid == -1)
  {
    mutt_perror(_("Can't open PGP subprocess!"));
    mutt_file_fclose(&fp);
    unlink(sigfile);
    unlink(signedfile);
    return NULL;
  }

  if (!pgp_use_gpg_agent())
    fputs(PgpPass, pgpin);
  fputc('\n', pgpin);
  mutt_file_fclose(&pgpin);

  /* Read back the PGP signature.  Also, change MESSAGE=>SIGNATURE as
   * recommended for future releases of PGP.
   */
  while (fgets(buffer, sizeof(buffer) - 1, pgpout) != NULL)
  {
    if (mutt_str_strcmp("-----BEGIN PGP MESSAGE-----\n", buffer) == 0)
      fputs("-----BEGIN PGP SIGNATURE-----\n", fp);
    else if (mutt_str_strcmp("-----END PGP MESSAGE-----\n", buffer) == 0)
      fputs("-----END PGP SIGNATURE-----\n", fp);
    else
      fputs(buffer, fp);
    empty = false; /* got some output, so we're ok */
  }

  /* check for errors from PGP */
  err = false;
  while (fgets(buffer, sizeof(buffer) - 1, pgperr) != NULL)
  {
    err = true;
    fputs(buffer, stdout);
  }

  if (mutt_wait_filter(thepid) && PgpCheckExit)
    empty = true;

  mutt_file_fclose(&pgperr);
  mutt_file_fclose(&pgpout);
  unlink(signedfile);

  if (fclose(fp) != 0)
  {
    mutt_perror("fclose");
    unlink(sigfile);
    return NULL;
  }

  if (err)
    mutt_any_key_to_continue(NULL);
  if (empty)
  {
    unlink(sigfile);
    /* most likely error is a bad passphrase, so automatically forget it */
    pgp_class_void_passphrase();
    return NULL; /* fatal error while signing */
  }

  t = mutt_body_new();
  t->type = TYPE_MULTIPART;
  t->subtype = mutt_str_strdup("signed");
  t->encoding = ENC_7BIT;
  t->use_disp = false;
  t->disposition = DISP_INLINE;

  mutt_generate_boundary(&t->parameter);
  mutt_param_set(&t->parameter, "protocol", "application/pgp-signature");
  mutt_param_set(&t->parameter, "micalg", pgp_micalg(sigfile));

  t->parts = a;
  a = t;

  t->parts->next = mutt_body_new();
  t = t->parts->next;
  t->type = TYPE_APPLICATION;
  t->subtype = mutt_str_strdup("pgp-signature");
  t->filename = mutt_str_strdup(sigfile);
  t->use_disp = false;
  t->disposition = DISP_NONE;
  t->encoding = ENC_7BIT;
  t->unlink = true; /* ok to remove this file after sending. */
  mutt_param_set(&t->parameter, "name", "signature.asc");

  return a;
}

/**
 * pgp_class_find_keys - Implements CryptModuleSpecs::find_keys()
 */
char *pgp_class_find_keys(struct Address *addrlist, bool oppenc_mode)
{
  struct ListHead crypt_hook_list = STAILQ_HEAD_INITIALIZER(crypt_hook_list);
  struct ListNode *crypt_hook = NULL;
  char *keyID = NULL, *keylist = NULL;
  size_t keylist_size = 0;
  size_t keylist_used = 0;
  struct Address *addr = NULL;
  struct Address *p = NULL, *q = NULL;
  struct PgpKeyInfo *k_info = NULL;
  char buf[LONG_STRING];
  int r;
  bool key_selected;

  const char *fqdn = mutt_fqdn(true);

  for (p = addrlist; p; p = p->next)
  {
    key_selected = false;
    mutt_crypt_hook(&crypt_hook_list, p);
    crypt_hook = STAILQ_FIRST(&crypt_hook_list);
    do
    {
      q = p;
      k_info = NULL;

      if (crypt_hook)
      {
        keyID = crypt_hook->data;
        r = MUTT_YES;
        if (!oppenc_mode && CryptConfirmhook)
        {
          snprintf(buf, sizeof(buf), _("Use keyID = \"%s\" for %s?"), keyID, p->mailbox);
          r = mutt_yesorno(buf, MUTT_YES);
        }
        if (r == MUTT_YES)
        {
          if (crypt_is_numerical_keyid(keyID))
          {
            if (strncmp(keyID, "0x", 2) == 0)
              keyID += 2;
            goto bypass_selection; /* you don't see this. */
          }

          /* check for e-mail address */
          if (strchr(keyID, '@') && (addr = mutt_addr_parse_list(NULL, keyID)))
          {
            if (fqdn)
              mutt_addr_qualify(addr, fqdn);
            q = addr;
          }
          else if (!oppenc_mode)
          {
            k_info = pgp_getkeybystr(keyID, KEYFLAG_CANENCRYPT, PGP_PUBRING);
          }
        }
        else if (r == MUTT_NO)
        {
          if (key_selected || STAILQ_NEXT(crypt_hook, entries))
          {
            crypt_hook = STAILQ_NEXT(crypt_hook, entries);
            continue;
          }
        }
        else if (r == MUTT_ABORT)
        {
          FREE(&keylist);
          mutt_addr_free(&addr);
          mutt_list_free(&crypt_hook_list);
          return NULL;
        }
      }

      if (!k_info)
      {
        pgp_class_invoke_getkeys(q);
        k_info = pgp_getkeybyaddr(q, KEYFLAG_CANENCRYPT, PGP_PUBRING, oppenc_mode);
      }

      if (!k_info && !oppenc_mode)
      {
        snprintf(buf, sizeof(buf), _("Enter keyID for %s: "), q->mailbox);
        k_info = pgp_ask_for_key(buf, q->mailbox, KEYFLAG_CANENCRYPT, PGP_PUBRING);
      }

      if (!k_info)
      {
        FREE(&keylist);
        mutt_addr_free(&addr);
        mutt_list_free(&crypt_hook_list);
        return NULL;
      }

      keyID = pgp_fpr_or_lkeyid(k_info);

    bypass_selection:
      keylist_size += mutt_str_strlen(keyID) + 4;
      mutt_mem_realloc(&keylist, keylist_size);
      sprintf(keylist + keylist_used, "%s0x%s", keylist_used ? " " : "", keyID);
      keylist_used = mutt_str_strlen(keylist);

      key_selected = true;

      pgp_free_key(&k_info);
      mutt_addr_free(&addr);

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
struct Body *pgp_class_encrypt_message(struct Body *a, char *keylist, bool sign)
{
  char buf[LONG_STRING];
  char tempfile[PATH_MAX];
  char pgpinfile[PATH_MAX];
  FILE *pgpin = NULL, *fptmp = NULL;
  struct Body *t = NULL;
  int err = 0;
  int empty = 0;
  pid_t thepid;

  mutt_mktemp(tempfile, sizeof(tempfile));
  FILE *fpout = mutt_file_fopen(tempfile, "w+");
  if (!fpout)
  {
    mutt_perror(tempfile);
    return NULL;
  }

  FILE *pgperr = mutt_file_mkstemp();
  if (!pgperr)
  {
    mutt_perror(_("Can't create temporary file"));
    unlink(tempfile);
    mutt_file_fclose(&fpout);
    return NULL;
  }

  mutt_mktemp(pgpinfile, sizeof(pgpinfile));
  fptmp = mutt_file_fopen(pgpinfile, "w");
  if (!fptmp)
  {
    mutt_perror(pgpinfile);
    unlink(tempfile);
    mutt_file_fclose(&fpout);
    mutt_file_fclose(&pgperr);
    return NULL;
  }

  if (sign)
    crypt_convert_to_7bit(a);

  mutt_write_mime_header(a, fptmp);
  fputc('\n', fptmp);
  mutt_write_mime_body(a, fptmp);
  mutt_file_fclose(&fptmp);

  thepid = pgp_invoke_encrypt(&pgpin, NULL, NULL, -1, fileno(fpout),
                              fileno(pgperr), pgpinfile, keylist, sign);
  if (thepid == -1)
  {
    mutt_file_fclose(&fpout);
    mutt_file_fclose(&pgperr);
    unlink(pgpinfile);
    return NULL;
  }

  if (sign)
  {
    if (!pgp_use_gpg_agent())
      fputs(PgpPass, pgpin);
    fputc('\n', pgpin);
  }
  mutt_file_fclose(&pgpin);

  if (mutt_wait_filter(thepid) && PgpCheckExit)
    empty = 1;

  unlink(pgpinfile);

  fflush(fpout);
  rewind(fpout);
  if (!empty)
    empty = (fgetc(fpout) == EOF);
  mutt_file_fclose(&fpout);

  fflush(pgperr);
  rewind(pgperr);
  while (fgets(buf, sizeof(buf) - 1, pgperr) != NULL)
  {
    err = 1;
    fputs(buf, stdout);
  }
  mutt_file_fclose(&pgperr);

  /* pause if there is any error output from PGP */
  if (err)
    mutt_any_key_to_continue(NULL);

  if (empty)
  {
    /* fatal error while trying to encrypt message */
    if (sign)
      pgp_class_void_passphrase(); /* just in case */
    unlink(tempfile);
    return NULL;
  }

  t = mutt_body_new();
  t->type = TYPE_MULTIPART;
  t->subtype = mutt_str_strdup("encrypted");
  t->encoding = ENC_7BIT;
  t->use_disp = false;
  t->disposition = DISP_INLINE;

  mutt_generate_boundary(&t->parameter);
  mutt_param_set(&t->parameter, "protocol", "application/pgp-encrypted");

  t->parts = mutt_body_new();
  t->parts->type = TYPE_APPLICATION;
  t->parts->subtype = mutt_str_strdup("pgp-encrypted");
  t->parts->encoding = ENC_7BIT;

  t->parts->next = mutt_body_new();
  t->parts->next->type = TYPE_APPLICATION;
  t->parts->next->subtype = mutt_str_strdup("octet-stream");
  t->parts->next->encoding = ENC_7BIT;
  t->parts->next->filename = mutt_str_strdup(tempfile);
  t->parts->next->use_disp = true;
  t->parts->next->disposition = DISP_ATTACH;
  t->parts->next->unlink = true; /* delete after sending the message */
  t->parts->next->d_filename = mutt_str_strdup("msg.asc"); /* non pgp/mime can save */

  return t;
}

/**
 * pgp_class_traditional_encryptsign - Implements CryptModuleSpecs::pgp_traditional_encryptsign()
 */
struct Body *pgp_class_traditional_encryptsign(struct Body *a, int flags, char *keylist)
{
  struct Body *b = NULL;

  char pgpoutfile[PATH_MAX];
  char pgpinfile[PATH_MAX];

  char body_charset[STRING];
  char *from_charset = NULL;
  const char *send_charset = NULL;

  FILE *fp = NULL;

  bool empty = false;
  bool err;

  char buf[STRING];

  pid_t thepid;

  if (a->type != TYPE_TEXT)
    return NULL;
  if (mutt_str_strcasecmp(a->subtype, "plain") != 0)
    return NULL;

  fp = fopen(a->filename, "r");
  if (!fp)
  {
    mutt_perror(a->filename);
    return NULL;
  }

  mutt_mktemp(pgpinfile, sizeof(pgpinfile));
  FILE *pgpin = mutt_file_fopen(pgpinfile, "w");
  if (!pgpin)
  {
    mutt_perror(pgpinfile);
    mutt_file_fclose(&fp);
    return NULL;
  }

  /* The following code is really correct:  If noconv is set,
   * a's charset parameter contains the on-disk character set, and
   * we have to convert from that to utf-8.  If noconv is not set,
   * we have to convert from $charset to utf-8.
   */

  mutt_body_get_charset(a, body_charset, sizeof(body_charset));
  if (a->noconv)
    from_charset = body_charset;
  else
    from_charset = Charset;

  if (!mutt_ch_is_us_ascii(body_charset))
  {
    int c;
    struct FgetConv *fc = NULL;

    if (flags & ENCRYPT)
      send_charset = "us-ascii";
    else
      send_charset = "utf-8";

    /* fromcode is assumed to be correct: we set flags to 0 */
    fc = mutt_ch_fgetconv_open(fp, from_charset, "utf-8", 0);
    while ((c = mutt_ch_fgetconv(fc)) != EOF)
      fputc(c, pgpin);

    mutt_ch_fgetconv_close(&fc);
  }
  else
  {
    send_charset = "us-ascii";
    mutt_file_copy_stream(fp, pgpin);
  }
  mutt_file_fclose(&fp);
  mutt_file_fclose(&pgpin);

  mutt_mktemp(pgpoutfile, sizeof(pgpoutfile));
  FILE *pgpout = mutt_file_fopen(pgpoutfile, "w+");
  FILE *pgperr = mutt_file_mkstemp();
  if (!pgpout || !pgperr)
  {
    mutt_perror(pgpout ? "Can't create temporary file" : pgpoutfile);
    unlink(pgpinfile);
    if (pgpout)
    {
      mutt_file_fclose(&pgpout);
      unlink(pgpoutfile);
    }
    mutt_file_fclose(&pgperr);
    return NULL;
  }

  thepid = pgp_invoke_traditional(&pgpin, NULL, NULL, -1, fileno(pgpout),
                                  fileno(pgperr), pgpinfile, keylist, flags);
  if (thepid == -1)
  {
    mutt_perror(_("Can't invoke PGP"));
    mutt_file_fclose(&pgpout);
    mutt_file_fclose(&pgperr);
    mutt_file_unlink(pgpinfile);
    unlink(pgpoutfile);
    return NULL;
  }

  if (pgp_use_gpg_agent())
    *PgpPass = 0;
  if (flags & SIGN)
    fprintf(pgpin, "%s\n", PgpPass);
  mutt_file_fclose(&pgpin);

  if (mutt_wait_filter(thepid) && PgpCheckExit)
    empty = true;

  mutt_file_unlink(pgpinfile);

  fflush(pgpout);
  fflush(pgperr);

  rewind(pgpout);
  rewind(pgperr);

  if (!empty)
    empty = (fgetc(pgpout) == EOF);
  mutt_file_fclose(&pgpout);

  err = false;

  while (fgets(buf, sizeof(buf), pgperr))
  {
    err = true;
    fputs(buf, stdout);
  }

  mutt_file_fclose(&pgperr);

  if (err)
    mutt_any_key_to_continue(NULL);

  if (empty)
  {
    if (flags & SIGN)
      pgp_class_void_passphrase(); /* just in case */
    unlink(pgpoutfile);
    return NULL;
  }

  b = mutt_body_new();

  b->encoding = ENC_7BIT;

  b->type = TYPE_TEXT;
  b->subtype = mutt_str_strdup("plain");

  mutt_param_set(&b->parameter, "x-action", (flags & ENCRYPT) ? "pgp-encrypted" : "pgp-signed");
  mutt_param_set(&b->parameter, "charset", send_charset);

  b->filename = mutt_str_strdup(pgpoutfile);

  b->disposition = DISP_NONE;
  b->unlink = true;

  b->noconv = true;
  b->use_disp = false;

  if (!(flags & ENCRYPT))
    b->encoding = a->encoding;

  return b;
}

/**
 * pgp_class_send_menu - Implements CryptModuleSpecs::send_menu()
 */
int pgp_class_send_menu(struct Header *msg)
{
  struct PgpKeyInfo *p = NULL;
  char *prompt = NULL, *letters = NULL, *choices = NULL;
  char promptbuf[LONG_STRING];
  int choice;

  if (!(WithCrypto & APPLICATION_PGP))
    return msg->security;

  /* If autoinline and no crypto options set, then set inline. */
  if (PgpAutoinline &&
      !((msg->security & APPLICATION_PGP) && (msg->security & (SIGN | ENCRYPT))))
  {
    msg->security |= INLINE;
  }

  msg->security |= APPLICATION_PGP;

  char *mime_inline = NULL;
  if (msg->security & INLINE)
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
   *       letter choices for those.
   */
  if (CryptOpportunisticEncrypt && (msg->security & OPPENCRYPT))
  {
    if (msg->security & (ENCRYPT | SIGN))
    {
      snprintf(promptbuf, sizeof(promptbuf),
               /* L10N: PGP options (inline) (opportunistic encryption is on) */
               _("PGP (s)ign, sign (a)s, %s format, (c)lear, or (o)ppenc mode "
                 "off? "),
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
      prompt = _("PGP (s)ign, sign (a)s, (c)lear, or (o)ppenc mode off? ");
      /* L10N: PGP options (opportunistic encryption is on) */
      letters = _("saco");
      choices = "SaCo";
    }
  }
  /* Opportunistic encryption option is set, but is toggled off
   * for this message.
   */
  else if (CryptOpportunisticEncrypt)
  {
    /* When the message is not selected for signing or encryption, the toggle
     * between PGP/MIME and Traditional doesn't make sense.
     */
    if (msg->security & (ENCRYPT | SIGN))
    {
      snprintf(promptbuf, sizeof(promptbuf),
               /* L10N: PGP options (inline) (opportunistic encryption is off) */
               _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, %s format, "
                 "(c)lear, or (o)ppenc mode? "),
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
                 "(o)ppenc mode? ");
      /* L10N: PGP options (opportunistic encryption is off) */
      letters = _("esabco");
      choices = "esabcO";
    }
  }
  /* Opportunistic encryption is unset */
  else
  {
    if (msg->security & (ENCRYPT | SIGN))
    {
      snprintf(promptbuf, sizeof(promptbuf),
               /* L10N: PGP options (inline) */
               _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, %s format, or "
                 "(c)lear? "),
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
      prompt = _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (c)lear? ");
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

        p = pgp_ask_for_key(_("Sign as: "), NULL, 0, PGP_SECRING);
        if (p)
        {
          char input_signas[SHORT_STRING];
          snprintf(input_signas, sizeof(input_signas), "0x%s", pgp_fpr_or_lkeyid(p));
          mutt_str_replace(&PgpSignAs, input_signas);
          pgp_free_key(&p);

          msg->security |= SIGN;

          crypt_pgp_void_passphrase(); /* probably need a different passphrase */
        }
        break;

      case 'b': /* (b)oth */
        msg->security |= (ENCRYPT | SIGN);
        break;

      case 'C':
        msg->security &= ~SIGN;
        break;

      case 'c': /* (c)lear     */
        msg->security &= ~(ENCRYPT | SIGN);
        break;

      case 'e': /* (e)ncrypt */
        msg->security |= ENCRYPT;
        msg->security &= ~SIGN;
        break;

      case 'i': /* toggle (i)nline */
        msg->security ^= INLINE;
        break;

      case 'O': /* oppenc mode on */
        msg->security |= OPPENCRYPT;
        crypt_opportunistic_encrypt(msg);
        break;

      case 'o': /* oppenc mode off */
        msg->security &= ~OPPENCRYPT;
        break;

      case 'S': /* (s)ign in oppenc mode */
        msg->security |= SIGN;
        break;

      case 's': /* (s)ign */
        msg->security &= ~ENCRYPT;
        msg->security |= SIGN;
        break;
    }
  }

  return msg->security;
}
