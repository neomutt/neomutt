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

/*
 * This file contains all of the PGP routines necessary to sign, encrypt,
 * verify and decrypt PGP messages in either the new PGP/MIME format, or
 * in the older Application/Pgp format.  It also contains some code to
 * cache the user's passphrase for repeat use when decrypting or signing
 * a message.
 */

#include "config.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "lib/lib.h"
#include "mutt.h"
#include "address.h"
#include "body.h"
#include "charset.h"
#include "crypt.h"
#include "cryptglue.h"
#include "filter.h"
#include "globals.h"
#include "header.h"
#include "mime.h"
#include "mutt_curses.h"
#include "mutt_regex.h"
#include "ncrypt.h"
#include "options.h"
#include "parameter.h"
#include "pgp.h"
#include "pgpinvoke.h"
#include "pgplib.h"
#include "pgpmicalg.h"
#include "protos.h"
#include "rfc822.h"
#include "state.h"

char PgpPass[LONG_STRING];
time_t PgpExptime = 0; /* when does the cached passphrase expire? */

void pgp_void_passphrase(void)
{
  memset(PgpPass, 0, sizeof(PgpPass));
  PgpExptime = 0;
}

int pgp_valid_passphrase(void)
{
  time_t now = time(NULL);

  if (pgp_use_gpg_agent())
  {
    *PgpPass = 0;
    return 1; /* handled by gpg-agent */
  }

  if (now < PgpExptime)
    /* Use cached copy.  */
    return 1;

  pgp_void_passphrase();

  if (mutt_get_password(_("Enter PGP passphrase:"), PgpPass, sizeof(PgpPass)) == 0)
  {
    PgpExptime = time(NULL) + PgpTimeout;
    return 1;
  }
  else
    PgpExptime = 0;

  return 0;
}

bool pgp_use_gpg_agent(void)
{
  char *tty = NULL;

  /* GnuPG 2.1 no longer exports GPG_AGENT_INFO */
  if (!option(OPT_PGP_USE_GPG_AGENT))
    return false;

  if ((tty = ttyname(0)))
  {
    setenv("GPG_TTY", tty, 0);
    mutt_envlist_set("GPG_TTY", tty, false);
  }

  return true;
}

static struct PgpKeyInfo *_pgp_parent(struct PgpKeyInfo *k)
{
  if ((k->flags & KEYFLAG_SUBKEY) && k->parent && option(OPT_PGP_IGNORE_SUBKEYS))
    k = k->parent;

  return k;
}

char *pgp_long_keyid(struct PgpKeyInfo *k)
{
  k = _pgp_parent(k);

  return k->keyid;
}

char *pgp_short_keyid(struct PgpKeyInfo *k)
{
  k = _pgp_parent(k);

  return k->keyid + 8;
}

char *pgp_keyid(struct PgpKeyInfo *k)
{
  k = _pgp_parent(k);

  return _pgp_keyid(k);
}

char *_pgp_keyid(struct PgpKeyInfo *k)
{
  if (option(OPT_PGP_LONG_IDS))
    return k->keyid;
  else
    return (k->keyid + 8);
}

static char *pgp_fingerprint(struct PgpKeyInfo *k)
{
  k = _pgp_parent(k);

  return k->fingerprint;
}

/**
 * pgp_fpr_or_lkeyid - Get the fingerprint or long keyid
 *
 * Grab the longest key identifier available: fingerprint or else
 * the long keyid.
 *
 * The longest available should be used for internally identifying
 * the key and for invoking pgp commands.
 */
char *pgp_fpr_or_lkeyid(struct PgpKeyInfo *k)
{
  char *fingerprint = NULL;

  fingerprint = pgp_fingerprint(k);
  return fingerprint ? fingerprint : pgp_long_keyid(k);
}

/* ----------------------------------------------------------------------------
 * Routines for handing PGP input.
 */

/**
 * pgp_copy_checksig - Copy PGP output and look for signs of a good signature
 */
static int pgp_copy_checksig(FILE *fpin, FILE *fpout)
{
  int rv = -1;

  if (PgpGoodSign.pattern)
  {
    char *line = NULL;
    int lineno = 0;
    size_t linelen;

    while ((line = mutt_read_line(line, &linelen, fpin, &lineno, 0)) != NULL)
    {
      if (regexec(PgpGoodSign.regex, line, 0, NULL, 0) == 0)
      {
        mutt_debug(2, "pgp_copy_checksig: \"%s\" matches regex.\n", line);
        rv = 0;
      }
      else
        mutt_debug(2, "pgp_copy_checksig: \"%s\" doesn't match regex.\n", line);

      if (strncmp(line, "[GNUPG:] ", 9) == 0)
        continue;
      fputs(line, fpout);
      fputc('\n', fpout);
    }
    FREE(&line);
  }
  else
  {
    mutt_debug(2, "pgp_copy_checksig: No pattern.\n");
    mutt_copy_stream(fpin, fpout);
    rv = 1;
  }

  return rv;
}

/**
 * pgp_check_decryption_okay - Check PGP output to look for successful outcome
 *
 * Checks PGP output messages to look for the $pgp_decryption_okay message.
 * This protects against messages with multipart/encrypted headers but which
 * aren't actually encrypted.  See ticket #3770
 */
static int pgp_check_decryption_okay(FILE *fpin)
{
  int rv = -1;

  if (PgpDecryptionOkay.pattern)
  {
    char *line = NULL;
    int lineno = 0;
    size_t linelen;

    while ((line = mutt_read_line(line, &linelen, fpin, &lineno, 0)) != NULL)
    {
      if (regexec(PgpDecryptionOkay.regex, line, 0, NULL, 0) == 0)
      {
        mutt_debug(2, "pgp_check_decryption_okay: \"%s\" matches regex.\n", line);
        rv = 0;
        break;
      }
      else
        mutt_debug(2,
                   "pgp_check_decryption_okay: \"%s\" doesn't match regex.\n", line);
    }
    FREE(&line);
  }
  else
  {
    mutt_debug(2, "pgp_check_decryption_okay: No pattern.\n");
    rv = 1;
  }

  return rv;
}

/**
 * pgp_copy_clearsigned - Copy a clearsigned message, stripping the signature
 *
 * XXX - charset handling: We assume that it is safe to do character set
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

  FGETCONV *fc = NULL;

  rewind(fpin);

  /* fromcode comes from the MIME Content-Type charset label. It might
   * be a wrong label, so we want the ability to do corrections via
   * charset-hooks. Therefore we set flags to MUTT_ICONV_HOOK_FROM.
   */
  fc = fgetconv_open(fpin, charset, Charset, MUTT_ICONV_HOOK_FROM);

  for (complete = true, armor_header = true; fgetconvs(buf, sizeof(buf), fc) != NULL;
       complete = (strchr(buf, '\n') != NULL))
  {
    if (!complete)
    {
      if (!armor_header)
        state_puts(buf, s);
      continue;
    }

    if (mutt_strcmp(buf, "-----BEGIN PGP SIGNATURE-----\n") == 0)
      break;

    if (armor_header)
    {
      char *p = mutt_skip_whitespace(buf);
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

  fgetconv_close(&fc);
}

/**
 * pgp_application_pgp_handler - Support for the Application/PGP Content Type
 */
int pgp_application_pgp_handler(struct Body *m, struct State *s)
{
  bool could_not_decrypt = false;
  int needpass = -1;
  bool pgp_keyblock = false;
  bool clearsign = false;
  int rv, rc;
  int c = 1; /* silence GCC warning */
  long bytes;
  LOFF_T last_pos, offset;
  char buf[HUGE_STRING];
  char outfile[_POSIX_PATH_MAX];
  char tmpfname[_POSIX_PATH_MAX];
  FILE *pgpout = NULL, *pgpin = NULL, *pgperr = NULL;
  FILE *tmpfp = NULL;
  pid_t thepid;

  bool maybe_goodsig = true;
  bool have_any_sigs = false;

  char *gpgcharset = NULL;
  char body_charset[STRING];
  mutt_get_body_charset(body_charset, sizeof(body_charset), m);

  rc = 0; /* silence false compiler warning if (s->flags & MUTT_DISPLAY) */

  fseeko(s->fpin, m->offset, SEEK_SET);
  last_pos = m->offset;

  for (bytes = m->length; bytes > 0;)
  {
    if (fgets(buf, sizeof(buf), s->fpin) == NULL)
      break;

    offset = ftello(s->fpin);
    bytes -= (offset - last_pos); /* don't rely on mutt_strlen(buf) */
    last_pos = offset;

    if (mutt_strncmp("-----BEGIN PGP ", buf, 15) == 0)
    {
      clearsign = false;

      if (mutt_strcmp("MESSAGE-----\n", buf + 15) == 0)
        needpass = 1;
      else if (mutt_strcmp("SIGNED MESSAGE-----\n", buf + 15) == 0)
      {
        clearsign = true;
        needpass = 0;
      }
      else if (mutt_strcmp("PUBLIC KEY BLOCK-----\n", buf + 15) == 0)
      {
        needpass = 0;
        pgp_keyblock = true;
      }
      else
      {
        /* XXX - we may wish to recode here */
        if (s->prefix)
          state_puts(s->prefix, s);
        state_puts(buf, s);
        continue;
      }

      have_any_sigs = have_any_sigs || (clearsign && (s->flags & MUTT_VERIFY));

      /* Copy PGP material to temporary file */
      mutt_mktemp(tmpfname, sizeof(tmpfname));
      tmpfp = safe_fopen(tmpfname, "w+");
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
        bytes -= (offset - last_pos); /* don't rely on mutt_strlen(buf) */
        last_pos = offset;

        fputs(buf, tmpfp);

        if ((needpass && (mutt_strcmp("-----END PGP MESSAGE-----\n", buf) == 0)) ||
            (!needpass && ((mutt_strcmp("-----END PGP SIGNATURE-----\n", buf) == 0) ||
                           (mutt_strcmp("-----END PGP PUBLIC KEY BLOCK-----\n", buf) == 0))))
          break;
        /* remember optional Charset: armor header as defined by RfC4880 */
        if (mutt_strncmp("Charset: ", buf, 9) == 0)
        {
          size_t l = 0;
          FREE(&gpgcharset);
          gpgcharset = safe_strdup(buf + 9);
          if ((l = mutt_strlen(gpgcharset)) > 0 && gpgcharset[l - 1] == '\n')
            gpgcharset[l - 1] = 0;
          if (!mutt_check_charset(gpgcharset, 0))
            mutt_str_replace(&gpgcharset, "UTF-8");
        }
      }

      /* leave tmpfp open in case we still need it - but flush it! */
      fflush(tmpfp);

      /* Invoke PGP if needed */
      if (!clearsign || (s->flags & MUTT_VERIFY))
      {
        mutt_mktemp(outfile, sizeof(outfile));
        pgpout = safe_fopen(outfile, "w+");
        if (!pgpout)
        {
          mutt_perror(outfile);
          safe_fclose(&tmpfp);
          FREE(&gpgcharset);
          return -1;
        }

        if ((thepid = pgp_invoke_decode(&pgpin, NULL, &pgperr, -1, fileno(pgpout),
                                        -1, tmpfname, needpass)) == -1)
        {
          safe_fclose(&pgpout);
          maybe_goodsig = false;
          pgpin = NULL;
          pgperr = NULL;
          state_attach_puts(
              _("[-- Error: unable to create PGP subprocess! --]\n"), s);
        }
        else /* PGP started successfully */
        {
          if (needpass)
          {
            if (!pgp_valid_passphrase())
              pgp_void_passphrase();
            if (pgp_use_gpg_agent())
              *PgpPass = 0;
            fprintf(pgpin, "%s\n", PgpPass);
          }

          safe_fclose(&pgpin);

          if (s->flags & MUTT_DISPLAY)
          {
            crypt_current_time(s, "PGP");
            rc = pgp_copy_checksig(pgperr, s->fpout);
          }

          safe_fclose(&pgperr);
          rv = mutt_wait_filter(thepid);

          if (s->flags & MUTT_DISPLAY)
          {
            if (rc == 0)
              have_any_sigs = true;
            /*
             * Sig is bad if
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
          pgp_void_passphrase();
        }

        if (could_not_decrypt && !(s->flags & MUTT_DISPLAY))
        {
          mutt_error(_("Could not decrypt PGP message"));
          mutt_sleep(1);
          rc = -1;
          goto out;
        }
      }

      /*
       * Now, copy cleartext to the screen.
       */

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
        FGETCONV *fc = NULL;
        int ch;
        char *expected_charset =
            gpgcharset && *gpgcharset ? gpgcharset : "utf-8";

        mutt_debug(4, "pgp: recoding inline from [%s] to [%s]\n", expected_charset, Charset);

        rewind(pgpout);
        state_set_prefix(s);
        fc = fgetconv_open(pgpout, expected_charset, Charset, MUTT_ICONV_HOOK_FROM);
        while ((ch = fgetconv(fc)) != EOF)
          state_prefix_putc(ch, s);
        fgetconv_close(&fc);
      }

      /*
       * Multiple PGP blocks can exist, so these need to be closed and
       * unlinked inside the loop.
       */
      safe_fclose(&tmpfp);
      mutt_unlink(tmpfname);
      if (pgpout)
      {
        safe_fclose(&pgpout);
        mutt_unlink(outfile);
      }

      if (s->flags & MUTT_DISPLAY)
      {
        state_putc('\n', s);
        if (needpass)
        {
          state_attach_puts(_("[-- END PGP MESSAGE --]\n"), s);
          if (could_not_decrypt)
            mutt_error(_("Could not decrypt PGP message"));
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
      /* XXX - we may wish to recode here */
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
    safe_fclose(&tmpfp);
    mutt_unlink(tmpfname);
  }
  if (pgpout)
  {
    safe_fclose(&pgpout);
    mutt_unlink(outfile);
  }

  FREE(&gpgcharset);

  if (needpass == -1)
  {
    state_attach_puts(
        _("[-- Error: could not find beginning of PGP message! --]\n\n"), s);
    return -1;
  }

  return rc;
}

static int pgp_check_traditional_one_body(FILE *fp, struct Body *b)
{
  char tempfile[_POSIX_PATH_MAX];
  char buf[HUGE_STRING];
  FILE *tfp = NULL;

  short sgn = 0;
  short enc = 0;
  short key = 0;

  if (b->type != TYPETEXT)
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
    if (mutt_strncmp("-----BEGIN PGP ", buf, 15) == 0)
    {
      if (mutt_strcmp("MESSAGE-----\n", buf + 15) == 0)
        enc = 1;
      else if (mutt_strcmp("SIGNED MESSAGE-----\n", buf + 15) == 0)
        sgn = 1;
      else if (mutt_strcmp("PUBLIC KEY BLOCK-----\n", buf + 15) == 0)
        key = 1;
    }
  }
  safe_fclose(&tfp);
  unlink(tempfile);

  if (!enc && !sgn && !key)
    return 0;

  /* fix the content type */

  mutt_set_parameter("format", "fixed", &b->parameter);
  if (enc)
    mutt_set_parameter("x-action", "pgp-encrypted", &b->parameter);
  else if (sgn)
    mutt_set_parameter("x-action", "pgp-signed", &b->parameter);
  else if (key)
    mutt_set_parameter("x-action", "pgp-keys", &b->parameter);

  return 1;
}

int pgp_check_traditional(FILE *fp, struct Body *b, int just_one)
{
  int rv = 0;
  int r;
  for (; b; b = b->next)
  {
    if (!just_one && is_multipart(b))
      rv = pgp_check_traditional(fp, b->parts, 0) || rv;
    else if (b->type == TYPETEXT)
    {
      if ((r = mutt_is_application_pgp(b)))
        rv = rv || r;
      else
        rv = pgp_check_traditional_one_body(fp, b) || rv;
    }

    if (just_one)
      break;
  }

  return rv;
}

int pgp_verify_one(struct Body *sigbdy, struct State *s, const char *tempfile)
{
  char sigfile[_POSIX_PATH_MAX], pgperrfile[_POSIX_PATH_MAX];
  FILE *fp = NULL, *pgpout = NULL, *pgperr = NULL;
  pid_t thepid;
  int badsig = -1;
  int rv;

  snprintf(sigfile, sizeof(sigfile), "%s.asc", tempfile);

  fp = safe_fopen(sigfile, "w");
  if (!fp)
  {
    mutt_perror(sigfile);
    return -1;
  }

  fseeko(s->fpin, sigbdy->offset, SEEK_SET);
  mutt_copy_bytes(s->fpin, fp, sigbdy->length);
  safe_fclose(&fp);

  mutt_mktemp(pgperrfile, sizeof(pgperrfile));
  pgperr = safe_fopen(pgperrfile, "w+");
  if (!pgperr)
  {
    mutt_perror(pgperrfile);
    unlink(sigfile);
    return -1;
  }

  crypt_current_time(s, "PGP");

  if ((thepid = pgp_invoke_verify(NULL, &pgpout, NULL, -1, -1, fileno(pgperr),
                                  tempfile, sigfile)) != -1)
  {
    if (pgp_copy_checksig(pgpout, s->fpout) >= 0)
      badsig = 0;

    safe_fclose(&pgpout);
    fflush(pgperr);
    rewind(pgperr);

    if (pgp_copy_checksig(pgperr, s->fpout) >= 0)
      badsig = 0;

    if ((rv = mutt_wait_filter(thepid)))
      badsig = -1;

    mutt_debug(1, "pgp_verify_one: mutt_wait_filter returned %d.\n", rv);
  }

  safe_fclose(&pgperr);

  state_attach_puts(_("[-- End of PGP output --]\n\n"), s);

  mutt_unlink(sigfile);
  mutt_unlink(pgperrfile);

  mutt_debug(1, "pgp_verify_one: returning %d.\n", badsig);

  return badsig;
}

/**
 * pgp_extract_keys_from_attachment - Extract pgp keys from messages/attachments
 */
static void pgp_extract_keys_from_attachment(FILE *fp, struct Body *top)
{
  struct State s;
  FILE *tempfp = NULL;
  char tempfname[_POSIX_PATH_MAX];

  mutt_mktemp(tempfname, sizeof(tempfname));
  tempfp = safe_fopen(tempfname, "w");
  if (!tempfp)
  {
    mutt_perror(tempfname);
    return;
  }

  memset(&s, 0, sizeof(struct State));

  s.fpin = fp;
  s.fpout = tempfp;

  mutt_body_handler(top, &s);

  safe_fclose(&tempfp);

  pgp_invoke_import(tempfname);
  mutt_any_key_to_continue(NULL);

  mutt_unlink(tempfname);
}

void pgp_extract_keys_from_attachment_list(FILE *fp, int tag, struct Body *top)
{
  if (!fp)
  {
    mutt_error(_("Internal error.  Please submit a bug report."));
    return;
  }

  mutt_endwin(NULL);
  set_option(OPT_DONT_HANDLE_PGP_KEYS);

  for (; top; top = top->next)
  {
    if (!tag || top->tagged)
      pgp_extract_keys_from_attachment(fp, top);

    if (!tag)
      break;
  }

  unset_option(OPT_DONT_HANDLE_PGP_KEYS);
}

static struct Body *pgp_decrypt_part(struct Body *a, struct State *s,
                                     FILE *fpout, struct Body *p)
{
  if (!a || !s || !fpout || !p)
    return NULL;

  char buf[LONG_STRING];
  FILE *pgpin = NULL, *pgpout = NULL, *pgperr = NULL, *pgptmp = NULL;
  struct stat info;
  struct Body *tattach = NULL;
  int len;
  char pgperrfile[_POSIX_PATH_MAX];
  char pgptmpfile[_POSIX_PATH_MAX];
  pid_t thepid;
  int rv;

  mutt_mktemp(pgperrfile, sizeof(pgperrfile));
  pgperr = safe_fopen(pgperrfile, "w+");
  if (!pgperr)
  {
    mutt_perror(pgperrfile);
    return NULL;
  }
  unlink(pgperrfile);

  mutt_mktemp(pgptmpfile, sizeof(pgptmpfile));
  pgptmp = safe_fopen(pgptmpfile, "w");
  if (!pgptmp)
  {
    mutt_perror(pgptmpfile);
    safe_fclose(&pgperr);
    return NULL;
  }

  /* Position the stream at the beginning of the body, and send the data to
   * the temporary file.
   */

  fseeko(s->fpin, a->offset, SEEK_SET);
  mutt_copy_bytes(s->fpin, pgptmp, a->length);
  safe_fclose(&pgptmp);

  if ((thepid = pgp_invoke_decrypt(&pgpin, &pgpout, NULL, -1, -1,
                                   fileno(pgperr), pgptmpfile)) == -1)
  {
    safe_fclose(&pgperr);
    unlink(pgptmpfile);
    if (s->flags & MUTT_DISPLAY)
      state_attach_puts(
          _("[-- Error: could not create a PGP subprocess! --]\n\n"), s);
    return NULL;
  }

  /* send the PGP passphrase to the subprocess.  Never do this if the
     agent is active, because this might lead to a passphrase send as
     the message. */
  if (!pgp_use_gpg_agent())
    fputs(PgpPass, pgpin);
  fputc('\n', pgpin);
  safe_fclose(&pgpin);

  /* Read the output from PGP, and make sure to change CRLF to LF, otherwise
   * read_mime_header has a hard time parsing the message.
   */
  while (fgets(buf, sizeof(buf) - 1, pgpout) != NULL)
  {
    len = mutt_strlen(buf);
    if (len > 1 && buf[len - 2] == '\r')
      strcpy(buf + len - 2, "\n");
    fputs(buf, fpout);
  }

  safe_fclose(&pgpout);
  rv = mutt_wait_filter(thepid);
  mutt_unlink(pgptmpfile);

  fflush(pgperr);
  rewind(pgperr);
  if (pgp_check_decryption_okay(pgperr) < 0)
  {
    mutt_error(_("Decryption failed"));
    pgp_void_passphrase();
    safe_fclose(&pgperr);
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
  safe_fclose(&pgperr);

  fflush(fpout);
  rewind(fpout);

  if (pgp_use_gpg_agent())
    mutt_need_hard_redraw();

  if (fgetc(fpout) == EOF)
  {
    mutt_error(_("Decryption failed"));
    pgp_void_passphrase();
    return NULL;
  }

  rewind(fpout);

  tattach = mutt_read_mime_header(fpout, 0);
  if (tattach)
  {
    /*
     * Need to set the length of this body part.
     */
    fstat(fileno(fpout), &info);
    tattach->length = info.st_size - tattach->offset;

    /* See if we need to recurse on this MIME part.  */

    mutt_parse_part(fpout, tattach);
  }

  return tattach;
}

int pgp_decrypt_mime(FILE *fpin, FILE **fpout, struct Body *b, struct Body **cur)
{
  char tempfile[_POSIX_PATH_MAX];
  struct State s;
  struct Body *p = b;
  bool need_decode = false;
  int saved_type;
  LOFF_T saved_offset;
  size_t saved_length;
  FILE *decoded_fp = NULL;
  int rv = 0;

  if (mutt_is_valid_multipart_pgp_encrypted(b))
    b = b->parts->next;
  else if (mutt_is_malformed_multipart_pgp_encrypted(b))
  {
    b = b->parts->next->next;
    need_decode = true;
  }
  else
    return -1;

  memset(&s, 0, sizeof(s));
  s.fpin = fpin;

  if (need_decode)
  {
    saved_type = b->type;
    saved_offset = b->offset;
    saved_length = b->length;

    mutt_mktemp(tempfile, sizeof(tempfile));
    decoded_fp = safe_fopen(tempfile, "w+");
    if (!decoded_fp)
    {
      mutt_perror(tempfile);
      return -1;
    }
    unlink(tempfile);

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

  mutt_mktemp(tempfile, sizeof(tempfile));
  *fpout = safe_fopen(tempfile, "w+");
  if (!*fpout)
  {
    mutt_perror(tempfile);
    rv = -1;
    goto bail;
  }
  unlink(tempfile);

  *cur = pgp_decrypt_part(b, &s, *fpout, p);
  if (!*cur)
    rv = -1;
  rewind(*fpout);

bail:
  if (need_decode)
  {
    b->type = saved_type;
    b->length = saved_length;
    b->offset = saved_offset;
    safe_fclose(&decoded_fp);
  }

  return rv;
}

/**
 * pgp_encrypted_handler - Handler of PGP encrypted data
 *
 * This handler is passed the application/octet-stream directly.
 * The caller must propagate a->goodsig to its parent.
 */
int pgp_encrypted_handler(struct Body *a, struct State *s)
{
  char tempfile[_POSIX_PATH_MAX];
  FILE *fpout = NULL, *fpin = NULL;
  struct Body *tattach = NULL;
  int rc = 0;

  mutt_mktemp(tempfile, sizeof(tempfile));
  fpout = safe_fopen(tempfile, "w+");
  if (!fpout)
  {
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
      state_attach_puts(
          _("[-- The following data is PGP/MIME encrypted --]\n\n"), s);

    fpin = s->fpin;
    s->fpin = fpout;
    rc = mutt_body_handler(tattach, s);
    s->fpin = fpin;

    /*
     * if a multipart/signed is the _only_ sub-part of a
     * multipart/encrypted, cache signature verification
     * status.
     *
     */

    if (mutt_is_multipart_signed(tattach) && !tattach->next)
      a->goodsig |= tattach->goodsig;

    if (s->flags & MUTT_DISPLAY)
    {
      state_puts("\n", s);
      state_attach_puts(_("[-- End of PGP/MIME encrypted data --]\n"), s);
    }

    mutt_free_body(&tattach);
    /* clear 'Invoking...' message, since there's no error */
    mutt_message(_("PGP message successfully decrypted."));
  }
  else
  {
    mutt_error(_("Could not decrypt PGP message"));
    mutt_sleep(2);
    /* void the passphrase, even if it's not necessarily the problem */
    pgp_void_passphrase();
    rc = -1;
  }

  safe_fclose(&fpout);
  mutt_unlink(tempfile);

  return rc;
}

/* ----------------------------------------------------------------------------
 * Routines for sending PGP/MIME messages.
 */

struct Body *pgp_sign_message(struct Body *a)
{
  struct Body *t = NULL;
  char buffer[LONG_STRING];
  char sigfile[_POSIX_PATH_MAX], signedfile[_POSIX_PATH_MAX];
  FILE *pgpin = NULL, *pgpout = NULL, *pgperr = NULL, *fp = NULL, *sfp = NULL;
  bool err = false;
  bool empty = true;
  pid_t thepid;

  convert_to_7bit(a); /* Signed data _must_ be in 7-bit format. */

  mutt_mktemp(sigfile, sizeof(sigfile));
  fp = safe_fopen(sigfile, "w");
  if (!fp)
  {
    return NULL;
  }

  mutt_mktemp(signedfile, sizeof(signedfile));
  sfp = safe_fopen(signedfile, "w");
  if (!sfp)
  {
    mutt_perror(signedfile);
    safe_fclose(&fp);
    unlink(sigfile);
    return NULL;
  }

  mutt_write_mime_header(a, sfp);
  fputc('\n', sfp);
  mutt_write_mime_body(a, sfp);
  safe_fclose(&sfp);

  thepid = pgp_invoke_sign(&pgpin, &pgpout, &pgperr, -1, -1, -1, signedfile);
  if (thepid == -1)
  {
    mutt_perror(_("Can't open PGP subprocess!"));
    safe_fclose(&fp);
    unlink(sigfile);
    unlink(signedfile);
    return NULL;
  }

  if (!pgp_use_gpg_agent())
    fputs(PgpPass, pgpin);
  fputc('\n', pgpin);
  safe_fclose(&pgpin);

  /*
   * Read back the PGP signature.  Also, change MESSAGE=>SIGNATURE as
   * recommended for future releases of PGP.
   */
  while (fgets(buffer, sizeof(buffer) - 1, pgpout) != NULL)
  {
    if (mutt_strcmp("-----BEGIN PGP MESSAGE-----\n", buffer) == 0)
      fputs("-----BEGIN PGP SIGNATURE-----\n", fp);
    else if (mutt_strcmp("-----END PGP MESSAGE-----\n", buffer) == 0)
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

  if (mutt_wait_filter(thepid) && option(OPT_PGP_CHECK_EXIT))
    empty = true;

  safe_fclose(&pgperr);
  safe_fclose(&pgpout);
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
    pgp_void_passphrase();
    return NULL; /* fatal error while signing */
  }

  t = mutt_new_body();
  t->type = TYPEMULTIPART;
  t->subtype = safe_strdup("signed");
  t->encoding = ENC7BIT;
  t->use_disp = false;
  t->disposition = DISPINLINE;

  mutt_generate_boundary(&t->parameter);
  mutt_set_parameter("protocol", "application/pgp-signature", &t->parameter);
  mutt_set_parameter("micalg", pgp_micalg(sigfile), &t->parameter);

  t->parts = a;
  a = t;

  t->parts->next = mutt_new_body();
  t = t->parts->next;
  t->type = TYPEAPPLICATION;
  t->subtype = safe_strdup("pgp-signature");
  t->filename = safe_strdup(sigfile);
  t->use_disp = false;
  t->disposition = DISPNONE;
  t->encoding = ENC7BIT;
  t->unlink = true; /* ok to remove this file after sending. */
  mutt_set_parameter("name", "signature.asc", &t->parameter);

  return a;
}

/**
 * pgp_find_keys - Find the keyids of the recipients of a message
 *
 * It returns NULL if any of the keys can not be found.  If oppenc_mode is
 * true, only keys that can be determined without prompting will be used.
 */
char *pgp_find_keys(struct Address *adrlist, int oppenc_mode)
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

  const char *fqdn = mutt_fqdn(1);

  for (p = adrlist; p; p = p->next)
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
        if (!oppenc_mode && option(OPT_CRYPT_CONFIRMHOOK))
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
          if (strchr(keyID, '@') && (addr = rfc822_parse_adrlist(NULL, keyID)))
          {
            if (fqdn)
              rfc822_qualify(addr, fqdn);
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
          rfc822_free_address(&addr);
          mutt_list_free(&crypt_hook_list);
          return NULL;
        }
      }

      if (!k_info)
      {
        pgp_invoke_getkeys(q);
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
        rfc822_free_address(&addr);
        mutt_list_free(&crypt_hook_list);
        return NULL;
      }

      keyID = pgp_fpr_or_lkeyid(k_info);

    bypass_selection:
      keylist_size += mutt_strlen(keyID) + 4;
      safe_realloc(&keylist, keylist_size);
      sprintf(keylist + keylist_used, "%s0x%s", keylist_used ? " " : "", keyID);
      keylist_used = mutt_strlen(keylist);

      key_selected = true;

      pgp_free_key(&k_info);
      rfc822_free_address(&addr);

      if (crypt_hook)
        crypt_hook = STAILQ_NEXT(crypt_hook, entries);

    } while (crypt_hook);

    mutt_list_free(&crypt_hook_list);
  }
  return keylist;
}

/**
 * pgp_encrypt_message - Encrypt a message
 *
 * Warning: "a" is no longer freed in this routine, you need to free it later.
 * This is necessary for $fcc_attach.
 */
struct Body *pgp_encrypt_message(struct Body *a, char *keylist, int sign)
{
  char buf[LONG_STRING];
  char tempfile[_POSIX_PATH_MAX], pgperrfile[_POSIX_PATH_MAX];
  char pgpinfile[_POSIX_PATH_MAX];
  FILE *pgpin = NULL, *pgperr = NULL, *fpout = NULL, *fptmp = NULL;
  struct Body *t = NULL;
  int err = 0;
  int empty = 0;
  pid_t thepid;

  mutt_mktemp(tempfile, sizeof(tempfile));
  fpout = safe_fopen(tempfile, "w+");
  if (!fpout)
  {
    mutt_perror(tempfile);
    return NULL;
  }

  mutt_mktemp(pgperrfile, sizeof(pgperrfile));
  pgperr = safe_fopen(pgperrfile, "w+");
  if (!pgperr)
  {
    mutt_perror(pgperrfile);
    unlink(tempfile);
    safe_fclose(&fpout);
    return NULL;
  }
  unlink(pgperrfile);

  mutt_mktemp(pgpinfile, sizeof(pgpinfile));
  fptmp = safe_fopen(pgpinfile, "w");
  if (!fptmp)
  {
    mutt_perror(pgpinfile);
    unlink(tempfile);
    safe_fclose(&fpout);
    safe_fclose(&pgperr);
    return NULL;
  }

  if (sign)
    convert_to_7bit(a);

  mutt_write_mime_header(a, fptmp);
  fputc('\n', fptmp);
  mutt_write_mime_body(a, fptmp);
  safe_fclose(&fptmp);

  if ((thepid = pgp_invoke_encrypt(&pgpin, NULL, NULL, -1, fileno(fpout),
                                   fileno(pgperr), pgpinfile, keylist, sign)) == -1)
  {
    safe_fclose(&fpout);
    safe_fclose(&pgperr);
    unlink(pgpinfile);
    return NULL;
  }

  if (sign)
  {
    if (!pgp_use_gpg_agent())
      fputs(PgpPass, pgpin);
    fputc('\n', pgpin);
  }
  safe_fclose(&pgpin);

  if (mutt_wait_filter(thepid) && option(OPT_PGP_CHECK_EXIT))
    empty = 1;

  unlink(pgpinfile);

  fflush(fpout);
  rewind(fpout);
  if (!empty)
    empty = (fgetc(fpout) == EOF);
  safe_fclose(&fpout);

  fflush(pgperr);
  rewind(pgperr);
  while (fgets(buf, sizeof(buf) - 1, pgperr) != NULL)
  {
    err = 1;
    fputs(buf, stdout);
  }
  safe_fclose(&pgperr);

  /* pause if there is any error output from PGP */
  if (err)
    mutt_any_key_to_continue(NULL);

  if (empty)
  {
    /* fatal error while trying to encrypt message */
    if (sign)
      pgp_void_passphrase(); /* just in case */
    unlink(tempfile);
    return NULL;
  }

  t = mutt_new_body();
  t->type = TYPEMULTIPART;
  t->subtype = safe_strdup("encrypted");
  t->encoding = ENC7BIT;
  t->use_disp = false;
  t->disposition = DISPINLINE;

  mutt_generate_boundary(&t->parameter);
  mutt_set_parameter("protocol", "application/pgp-encrypted", &t->parameter);

  t->parts = mutt_new_body();
  t->parts->type = TYPEAPPLICATION;
  t->parts->subtype = safe_strdup("pgp-encrypted");
  t->parts->encoding = ENC7BIT;

  t->parts->next = mutt_new_body();
  t->parts->next->type = TYPEAPPLICATION;
  t->parts->next->subtype = safe_strdup("octet-stream");
  t->parts->next->encoding = ENC7BIT;
  t->parts->next->filename = safe_strdup(tempfile);
  t->parts->next->use_disp = true;
  t->parts->next->disposition = DISPATTACH;
  t->parts->next->unlink = true; /* delete after sending the message */
  t->parts->next->d_filename = safe_strdup("msg.asc"); /* non pgp/mime can save */

  return t;
}

struct Body *pgp_traditional_encryptsign(struct Body *a, int flags, char *keylist)
{
  struct Body *b = NULL;

  char pgpoutfile[_POSIX_PATH_MAX];
  char pgperrfile[_POSIX_PATH_MAX];
  char pgpinfile[_POSIX_PATH_MAX];

  char body_charset[STRING];
  char *from_charset = NULL;
  const char *send_charset = NULL;

  FILE *pgpout = NULL, *pgperr = NULL, *pgpin = NULL;
  FILE *fp = NULL;

  bool empty = false;
  bool err;

  char buff[STRING];

  pid_t thepid;

  if (a->type != TYPETEXT)
    return NULL;
  if (mutt_strcasecmp(a->subtype, "plain") != 0)
    return NULL;

  fp = fopen(a->filename, "r");
  if (!fp)
  {
    mutt_perror(a->filename);
    return NULL;
  }

  mutt_mktemp(pgpinfile, sizeof(pgpinfile));
  pgpin = safe_fopen(pgpinfile, "w");
  if (!pgpin)
  {
    mutt_perror(pgpinfile);
    safe_fclose(&fp);
    return NULL;
  }

  /* The following code is really correct:  If noconv is set,
   * a's charset parameter contains the on-disk character set, and
   * we have to convert from that to utf-8.  If noconv is not set,
   * we have to convert from $charset to utf-8.
   */

  mutt_get_body_charset(body_charset, sizeof(body_charset), a);
  if (a->noconv)
    from_charset = body_charset;
  else
    from_charset = Charset;

  if (!mutt_is_us_ascii(body_charset))
  {
    int c;
    FGETCONV *fc = NULL;

    if (flags & ENCRYPT)
      send_charset = "us-ascii";
    else
      send_charset = "utf-8";

    /* fromcode is assumed to be correct: we set flags to 0 */
    fc = fgetconv_open(fp, from_charset, "utf-8", 0);
    while ((c = fgetconv(fc)) != EOF)
      fputc(c, pgpin);

    fgetconv_close(&fc);
  }
  else
  {
    send_charset = "us-ascii";
    mutt_copy_stream(fp, pgpin);
  }
  safe_fclose(&fp);
  safe_fclose(&pgpin);

  mutt_mktemp(pgpoutfile, sizeof(pgpoutfile));
  mutt_mktemp(pgperrfile, sizeof(pgperrfile));
  if ((pgpout = safe_fopen(pgpoutfile, "w+")) == NULL ||
      (pgperr = safe_fopen(pgperrfile, "w+")) == NULL)
  {
    mutt_perror(pgpout ? pgperrfile : pgpoutfile);
    unlink(pgpinfile);
    if (pgpout)
    {
      safe_fclose(&pgpout);
      unlink(pgpoutfile);
    }
    return NULL;
  }

  unlink(pgperrfile);

  if ((thepid = pgp_invoke_traditional(&pgpin, NULL, NULL, -1, fileno(pgpout),
                                       fileno(pgperr), pgpinfile, keylist, flags)) == -1)
  {
    mutt_perror(_("Can't invoke PGP"));
    safe_fclose(&pgpout);
    safe_fclose(&pgperr);
    mutt_unlink(pgpinfile);
    unlink(pgpoutfile);
    return NULL;
  }

  if (pgp_use_gpg_agent())
    *PgpPass = 0;
  if (flags & SIGN)
    fprintf(pgpin, "%s\n", PgpPass);
  safe_fclose(&pgpin);

  if (mutt_wait_filter(thepid) && option(OPT_PGP_CHECK_EXIT))
    empty = true;

  mutt_unlink(pgpinfile);

  fflush(pgpout);
  fflush(pgperr);

  rewind(pgpout);
  rewind(pgperr);

  if (!empty)
    empty = (fgetc(pgpout) == EOF);
  safe_fclose(&pgpout);

  err = false;

  while (fgets(buff, sizeof(buff), pgperr))
  {
    err = true;
    fputs(buff, stdout);
  }

  safe_fclose(&pgperr);

  if (err)
    mutt_any_key_to_continue(NULL);

  if (empty)
  {
    if (flags & SIGN)
      pgp_void_passphrase(); /* just in case */
    unlink(pgpoutfile);
    return NULL;
  }

  b = mutt_new_body();

  b->encoding = ENC7BIT;

  b->type = TYPETEXT;
  b->subtype = safe_strdup("plain");

  mutt_set_parameter("x-action",
                     flags & ENCRYPT ? "pgp-encrypted" : "pgp-signed", &b->parameter);
  mutt_set_parameter("charset", send_charset, &b->parameter);

  b->filename = safe_strdup(pgpoutfile);

  b->disposition = DISPNONE;
  b->unlink = true;

  b->noconv = true;
  b->use_disp = false;

  if (!(flags & ENCRYPT))
    b->encoding = a->encoding;

  return b;
}

int pgp_send_menu(struct Header *msg)
{
  struct PgpKeyInfo *p = NULL;
  char input_signas[SHORT_STRING];
  char *prompt = NULL, *letters = NULL, *choices = NULL;
  char promptbuf[LONG_STRING];
  int choice;

  if (!(WithCrypto & APPLICATION_PGP))
    return msg->security;

  /* If autoinline and no crypto options set, then set inline. */
  if (option(OPT_PGP_AUTOINLINE) &&
      !((msg->security & APPLICATION_PGP) && (msg->security & (SIGN | ENCRYPT))))
    msg->security |= INLINE;

  msg->security |= APPLICATION_PGP;

  char *mime_inline = NULL;
  if (msg->security & INLINE)
  {
    /* L10N: These next string MUST have the same highlighted letter
             One of them will appear in each of the three strings marked "(inline"), below. */
    mime_inline = _("PGP/M(i)ME");
  }
  else
  {
    /* L10N: These previous string MUST have the same highlighted letter
             One of them will appear in each of the three strings marked "(inline"), below. */
    mime_inline = _("(i)nline");
  }
  /*
   * Opportunistic encrypt is controlling encryption.  Allow to toggle
   * between inline and mime, but not turn encryption on or off.
   * NOTE: "Signing" and "Clearing" only adjust the sign bit, so we have different
   *       letter choices for those.
   */
  if (option(OPT_CRYPT_OPPORTUNISTIC_ENCRYPT) && (msg->security & OPPENCRYPT))
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
  /*
   * Opportunistic encryption option is set, but is toggled off
   * for this message.
   */
  else if (option(OPT_CRYPT_OPPORTUNISTIC_ENCRYPT))
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
  /*
   * Opportunistic encryption is unset
   */
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
        unset_option(OPT_PGP_CHECK_TRUST);

        if ((p = pgp_ask_for_key(_("Sign as: "), NULL, 0, PGP_SECRING)))
        {
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

      case 'c': /* (c)lear     */
        msg->security &= ~(ENCRYPT | SIGN);
        break;

      case 'C':
        msg->security &= ~SIGN;
        break;

      case 'O': /* oppenc mode on */
        msg->security |= OPPENCRYPT;
        crypt_opportunistic_encrypt(msg);
        break;

      case 'o': /* oppenc mode off */
        msg->security &= ~OPPENCRYPT;
        break;

      case 'i': /* toggle (i)nline */
        msg->security ^= INLINE;
        break;
    }
  }

  return msg->security;
}
