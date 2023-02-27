/**
 * @file
 * Miscellaneous functions for sending an email
 *
 * @authors
 * Copyright (C) 1996-2002,2009-2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
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
 * @page send_sendlib Miscellaneous functions for sending an email
 *
 * Miscellaneous functions for sending an email
 */

#include "config.h"
#include <inttypes.h> // IWYU pragma: keep
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "sendlib.h"
#include "lib.h"
#include "attach/lib.h"
#include "convert/lib.h"
#include "ncrypt/lib.h"
#include "copy.h"
#include "handler.h"
#include "mutt_globals.h"
#include "mutt_mailbox.h"
#include "muttlib.h"
#include "mx.h"
#include "options.h"

/**
 * mutt_lookup_mime_type - Find the MIME type for an attachment
 * @param att  Email with attachment
 * @param path Path to attachment
 * @retval num MIME type, e.g. #TYPE_IMAGE
 *
 * Given a file at 'path', see if there is a registered MIME type.
 * Returns the major MIME type, and copies the subtype to "d".  First look
 * in a system mime.types if we can find one, then look for ~/.mime.types.
 * The longest match is used so that we can match 'ps.gz' when 'gz' also
 * exists.
 */
enum ContentType mutt_lookup_mime_type(struct Body *att, const char *path)
{
  FILE *fp = NULL;
  char *p = NULL, *q = NULL, *ct = NULL;
  char buf[PATH_MAX] = { 0 };
  char subtype[256] = { 0 };
  char xtype[256] = { 0 };
  int sze, cur_sze = 0;
  bool found_mimetypes = false;
  enum ContentType type = TYPE_OTHER;

  int szf = mutt_str_len(path);

  for (int count = 0; count < 4; count++)
  {
    /* can't use strtok() because we use it in an inner loop below, so use
     * a switch statement here instead.  */
    switch (count)
    {
      /* last file with last entry to match wins type/xtype */
      case 0:
        /* check default unix mimetypes location first */
        mutt_str_copy(buf, "/etc/mime.types", sizeof(buf));
        break;
      case 1:
        mutt_str_copy(buf, SYSCONFDIR "/mime.types", sizeof(buf));
        break;
      case 2:
        mutt_str_copy(buf, PKGDATADIR "/mime.types", sizeof(buf));
        break;
      case 3:
        snprintf(buf, sizeof(buf), "%s/.mime.types", NONULL(HomeDir));
        break;
      default:
        mutt_debug(LL_DEBUG1, "Internal error, count = %d\n", count);
        goto bye; /* shouldn't happen */
    }

    fp = fopen(buf, "r");
    if (fp)
    {
      found_mimetypes = true;

      while (fgets(buf, sizeof(buf) - 1, fp))
      {
        /* weed out any comments */
        p = strchr(buf, '#');
        if (p)
          *p = '\0';

        /* remove any leading space. */
        ct = buf;
        SKIPWS(ct);

        /* position on the next field in this line */
        p = strpbrk(ct, " \t");
        if (!p)
          continue;
        *p++ = 0;
        SKIPWS(p);

        /* cycle through the file extensions */
        while ((p = strtok(p, " \t\n")))
        {
          sze = mutt_str_len(p);
          if ((sze > cur_sze) && (szf >= sze) && mutt_istr_equal(path + szf - sze, p) &&
              ((szf == sze) || (path[szf - sze - 1] == '.')))
          {
            /* get the content-type */

            p = strchr(ct, '/');
            if (!p)
            {
              /* malformed line, just skip it. */
              break;
            }
            *p++ = 0;

            for (q = p; *q && !IS_SPACE(*q); q++)
              ; // do nothing

            mutt_strn_copy(subtype, p, q - p, sizeof(subtype));

            type = mutt_check_mime_type(ct);
            if (type == TYPE_OTHER)
              mutt_str_copy(xtype, ct, sizeof(xtype));

            cur_sze = sze;
          }
          p = NULL;
        }
      }
      mutt_file_fclose(&fp);
    }
  }

bye:

  /* no mime.types file found */
  if (!found_mimetypes)
  {
    mutt_error(_("Could not find any mime.types file"));
  }

  if ((type != TYPE_OTHER) || (*xtype != '\0'))
  {
    att->type = type;
    mutt_str_replace(&att->subtype, subtype);
    mutt_str_replace(&att->xtype, xtype);
  }

  return type;
}

/**
 * transform_to_7bit - Convert MIME parts to 7-bit
 * @param a     Body of the email
 * @param fp_in File to read
 * @param sub   Config Subset
 */
static void transform_to_7bit(struct Body *a, FILE *fp_in, struct ConfigSubset *sub)
{
  struct Buffer *buf = NULL;
  struct State s = { 0 };
  struct stat st = { 0 };

  for (; a; a = a->next)
  {
    if (a->type == TYPE_MULTIPART)
    {
      a->encoding = ENC_7BIT;
      transform_to_7bit(a->parts, fp_in, sub);
    }
    else if (mutt_is_message_type(a->type, a->subtype))
    {
      mutt_message_to_7bit(a, fp_in, sub);
    }
    else
    {
      a->noconv = true;
      a->force_charset = true;

      /* Because of the potential recursion in message types, we
       * restrict the lifetime of the buffer tightly */
      buf = mutt_buffer_pool_get();
      mutt_buffer_mktemp(buf);
      s.fp_out = mutt_file_fopen(mutt_buffer_string(buf), "w");
      if (!s.fp_out)
      {
        mutt_perror("fopen");
        mutt_buffer_pool_release(&buf);
        return;
      }
      s.fp_in = fp_in;
      mutt_decode_attachment(a, &s);
      mutt_file_fclose(&s.fp_out);
      FREE(&a->d_filename);
      a->d_filename = a->filename;
      a->filename = mutt_buffer_strdup(buf);
      mutt_buffer_pool_release(&buf);
      a->unlink = true;
      if (stat(a->filename, &st) == -1)
      {
        mutt_perror("stat");
        return;
      }
      a->length = st.st_size;

      mutt_update_encoding(a, sub);
      if (a->encoding == ENC_8BIT)
        a->encoding = ENC_QUOTED_PRINTABLE;
      else if (a->encoding == ENC_BINARY)
        a->encoding = ENC_BASE64;
    }
  }
}

/**
 * mutt_message_to_7bit - Convert an email's MIME parts to 7-bit
 * @param a   Body of the email
 * @param fp  File to read (OPTIONAL)
 * @param sub Config Subset
 */
void mutt_message_to_7bit(struct Body *a, FILE *fp, struct ConfigSubset *sub)
{
  struct Buffer temp = mutt_buffer_make(0);
  FILE *fp_in = NULL;
  FILE *fp_out = NULL;
  struct stat st = { 0 };

  if (!a->filename && fp)
    fp_in = fp;
  else if (!a->filename || !(fp_in = fopen(a->filename, "r")))
  {
    mutt_error(_("Could not open %s"), a->filename ? a->filename : "(null)");
    return;
  }
  else
  {
    a->offset = 0;
    if (stat(a->filename, &st) == -1)
    {
      mutt_perror("stat");
      mutt_file_fclose(&fp_in);
      goto cleanup;
    }
    a->length = st.st_size;
  }

  /* Avoid buffer pool due to recursion */
  mutt_buffer_mktemp(&temp);
  fp_out = mutt_file_fopen(mutt_buffer_string(&temp), "w+");
  if (!fp_out)
  {
    mutt_perror("fopen");
    goto cleanup;
  }

  if (!mutt_file_seek(fp_in, a->offset, SEEK_SET))
  {
    goto cleanup;
  }
  a->parts = mutt_rfc822_parse_message(fp_in, a);

  transform_to_7bit(a->parts, fp_in, sub);

  mutt_copy_hdr(fp_in, fp_out, a->offset, a->offset + a->length,
                CH_MIME | CH_NONEWLINE | CH_XMIT, NULL, 0);

  fputs("MIME-Version: 1.0\n", fp_out);
  mutt_write_mime_header(a->parts, fp_out, sub);
  fputc('\n', fp_out);
  mutt_write_mime_body(a->parts, fp_out, sub);

  if (fp_in != fp)
    mutt_file_fclose(&fp_in);
  mutt_file_fclose(&fp_out);

  a->encoding = ENC_7BIT;
  FREE(&a->d_filename);
  a->d_filename = a->filename;
  if (a->filename && a->unlink)
    unlink(a->filename);
  a->filename = mutt_buffer_strdup(&temp);
  a->unlink = true;
  if (stat(a->filename, &st) == -1)
  {
    mutt_perror("stat");
    goto cleanup;
  }
  a->length = st.st_size;
  mutt_body_free(&a->parts);
  a->email->body = NULL;

cleanup:
  if (fp_in && (fp_in != fp))
    mutt_file_fclose(&fp_in);

  if (fp_out)
  {
    mutt_file_fclose(&fp_out);
    mutt_file_unlink(mutt_buffer_string(&temp));
  }

  mutt_buffer_dealloc(&temp);
}

/**
 * set_encoding - Determine which Content-Transfer-Encoding to use
 * @param[in]  b    Body of email
 * @param[out] info Info about the email
 * @param[in]  sub  Config Subset
 */
static void set_encoding(struct Body *b, struct Content *info, struct ConfigSubset *sub)
{
  const bool c_allow_8bit = cs_subset_bool(sub, "allow_8bit");
  if (b->type == TYPE_TEXT)
  {
    const bool c_encode_from = cs_subset_bool(sub, "encode_from");
    char send_charset[128] = { 0 };
    char *chsname = mutt_body_get_charset(b, send_charset, sizeof(send_charset));
    if ((info->lobin && !mutt_istr_startswith(chsname, "iso-2022")) ||
        (info->linemax > 990) || (info->from && c_encode_from))
    {
      b->encoding = ENC_QUOTED_PRINTABLE;
    }
    else if (info->hibin)
    {
      b->encoding = c_allow_8bit ? ENC_8BIT : ENC_QUOTED_PRINTABLE;
    }
    else
    {
      b->encoding = ENC_7BIT;
    }
  }
  else if ((b->type == TYPE_MESSAGE) || (b->type == TYPE_MULTIPART))
  {
    if (info->lobin || info->hibin)
    {
      if (c_allow_8bit && !info->lobin)
        b->encoding = ENC_8BIT;
      else
        mutt_message_to_7bit(b, NULL, sub);
    }
    else
      b->encoding = ENC_7BIT;
  }
  else if ((b->type == TYPE_APPLICATION) && mutt_istr_equal(b->subtype, "pgp-keys"))
  {
    b->encoding = ENC_7BIT;
  }
  else
  {
    /* Determine which encoding is smaller  */
    if (1.33 * (float) (info->lobin + info->hibin + info->ascii) <
        3.0 * (float) (info->lobin + info->hibin) + (float) info->ascii)
    {
      b->encoding = ENC_BASE64;
    }
    else
    {
      b->encoding = ENC_QUOTED_PRINTABLE;
    }
  }
}

/**
 * mutt_stamp_attachment - Timestamp an Attachment
 * @param a Attachment
 */
void mutt_stamp_attachment(struct Body *a)
{
  a->stamp = mutt_date_now();
}

/**
 * mutt_update_encoding - Update the encoding type
 * @param a   Body to update
 * @param sub Config Subset
 *
 * Assumes called from send mode where Body->filename points to actual file
 */
void mutt_update_encoding(struct Body *a, struct ConfigSubset *sub)
{
  struct Content *info = NULL;
  char chsbuf[256] = { 0 };

  /* override noconv when it's us-ascii */
  if (mutt_ch_is_us_ascii(mutt_body_get_charset(a, chsbuf, sizeof(chsbuf))))
    a->noconv = false;

  if (!a->force_charset && !a->noconv)
    mutt_param_delete(&a->parameter, "charset");

  info = mutt_get_content_info(a->filename, a, sub);
  if (!info)
    return;

  set_encoding(a, info, sub);
  mutt_stamp_attachment(a);

  FREE(&a->content);
  a->content = info;
}

/**
 * mutt_make_message_attach - Create a message attachment
 * @param m          Mailbox
 * @param e          Email
 * @param attach_msg true if attaching a message
 * @param sub        Config Subset
 * @retval ptr  Newly allocated Body
 * @retval NULL Error
 */
struct Body *mutt_make_message_attach(struct Mailbox *m, struct Email *e,
                                      bool attach_msg, struct ConfigSubset *sub)
{
  struct Body *body = NULL;
  FILE *fp = NULL;
  CopyMessageFlags cmflags;
  SecurityFlags pgp = WithCrypto ? e->security : SEC_NO_FLAGS;

  const bool c_mime_forward_decode = cs_subset_bool(sub, "mime_forward_decode");
  const bool c_forward_decrypt = cs_subset_bool(sub, "forward_decrypt");
  if (WithCrypto)
  {
    if ((c_mime_forward_decode || c_forward_decrypt) && (e->security & SEC_ENCRYPT))
    {
      if (!crypt_valid_passphrase(e->security))
        return NULL;
    }
  }

  struct Buffer *buf = mutt_buffer_pool_get();
  mutt_buffer_mktemp(buf);
  fp = mutt_file_fopen(mutt_buffer_string(buf), "w+");
  if (!fp)
  {
    mutt_buffer_pool_release(&buf);
    return NULL;
  }

  body = mutt_body_new();
  body->type = TYPE_MESSAGE;
  body->subtype = mutt_str_dup("rfc822");
  body->filename = mutt_str_dup(mutt_buffer_string(buf));
  body->unlink = true;
  body->use_disp = false;
  body->disposition = DISP_INLINE;
  body->noconv = true;

  mutt_buffer_pool_release(&buf);

  struct Message *msg = mx_msg_open(m, e->msgno);
  if (!msg)
  {
    mutt_body_free(&body);
    mutt_file_fclose(&fp);
    return NULL;
  }
  mutt_parse_mime_message(e, msg->fp);

  CopyHeaderFlags chflags = CH_XMIT;
  cmflags = MUTT_CM_NO_FLAGS;

  /* If we are attaching a message, ignore `$mime_forward_decode` */
  if (!attach_msg && c_mime_forward_decode)
  {
    chflags |= CH_MIME | CH_TXTPLAIN;
    cmflags = MUTT_CM_DECODE | MUTT_CM_CHARCONV;
    if (WithCrypto & APPLICATION_PGP)
      pgp &= ~PGP_ENCRYPT;
    if (WithCrypto & APPLICATION_SMIME)
      pgp &= ~SMIME_ENCRYPT;
  }
  else if ((WithCrypto != 0) && c_forward_decrypt && (e->security & SEC_ENCRYPT))
  {
    if (((WithCrypto & APPLICATION_PGP) != 0) && mutt_is_multipart_encrypted(e->body))
    {
      chflags |= CH_MIME | CH_NONEWLINE;
      cmflags = MUTT_CM_DECODE_PGP;
      pgp &= ~PGP_ENCRYPT;
    }
    else if (((WithCrypto & APPLICATION_PGP) != 0) &&
             ((mutt_is_application_pgp(e->body) & PGP_ENCRYPT) == PGP_ENCRYPT))
    {
      chflags |= CH_MIME | CH_TXTPLAIN;
      cmflags = MUTT_CM_DECODE | MUTT_CM_CHARCONV;
      pgp &= ~PGP_ENCRYPT;
    }
    else if (((WithCrypto & APPLICATION_SMIME) != 0) &&
             ((mutt_is_application_smime(e->body) & SMIME_ENCRYPT) == SMIME_ENCRYPT))
    {
      chflags |= CH_MIME | CH_TXTPLAIN;
      cmflags = MUTT_CM_DECODE | MUTT_CM_CHARCONV;
      pgp &= ~SMIME_ENCRYPT;
    }
  }

  mutt_copy_message(fp, e, msg, cmflags, chflags, 0);
  mx_msg_close(m, &msg);

  fflush(fp);
  rewind(fp);

  body->email = email_new();
  body->email->offset = 0;
  /* we don't need the user headers here */
  body->email->env = mutt_rfc822_read_header(fp, body->email, false, false);
  if (WithCrypto)
    body->email->security = pgp;
  mutt_update_encoding(body, sub);
  body->parts = body->email->body;

  mutt_file_fclose(&fp);

  return body;
}

/**
 * run_mime_type_query - Run an external command to determine the MIME type
 * @param att Attachment
 * @param sub Config Subset
 *
 * The command in $mime_type_query_command is run.
 */
static void run_mime_type_query(struct Body *att, struct ConfigSubset *sub)
{
  FILE *fp = NULL, *fp_err = NULL;
  char *buf = NULL;
  size_t buflen;
  pid_t pid;
  struct Buffer *cmd = mutt_buffer_pool_get();

  const char *const c_mime_type_query_command = cs_subset_string(sub, "mime_type_query_command");

  mutt_buffer_file_expand_fmt_quote(cmd, c_mime_type_query_command, att->filename);

  pid = filter_create(mutt_buffer_string(cmd), NULL, &fp, &fp_err);
  if (pid < 0)
  {
    mutt_error(_("Error running \"%s\""), mutt_buffer_string(cmd));
    mutt_buffer_pool_release(&cmd);
    return;
  }
  mutt_buffer_pool_release(&cmd);

  buf = mutt_file_read_line(buf, &buflen, fp, NULL, MUTT_RL_NO_FLAGS);
  if (buf)
  {
    if (strchr(buf, '/'))
      mutt_parse_content_type(buf, att);
    FREE(&buf);
  }

  mutt_file_fclose(&fp);
  mutt_file_fclose(&fp_err);
  filter_wait(pid);
}

/**
 * mutt_make_file_attach - Create a file attachment
 * @param path File to attach
 * @param sub  Config Subset
 * @retval ptr  Newly allocated Body
 * @retval NULL Error
 */
struct Body *mutt_make_file_attach(const char *path, struct ConfigSubset *sub)
{
  if (!path || (path[0] == '\0'))
    return NULL;

  struct Body *att = mutt_body_new();
  att->filename = mutt_str_dup(path);

  const char *const c_mime_type_query_command = cs_subset_string(sub, "mime_type_query_command");
  const bool c_mime_type_query_first = cs_subset_bool(sub, "mime_type_query_first");

  if (c_mime_type_query_command && c_mime_type_query_first)
    run_mime_type_query(att, sub);

  /* Attempt to determine the appropriate content-type based on the filename
   * suffix.  */
  if (!att->subtype)
    mutt_lookup_mime_type(att, path);

  if (!att->subtype && c_mime_type_query_command && !c_mime_type_query_first)
  {
    run_mime_type_query(att, sub);
  }

  struct Content *info = mutt_get_content_info(path, att, sub);
  if (!info)
  {
    mutt_body_free(&att);
    return NULL;
  }

  if (!att->subtype)
  {
    if ((info->nulbin == 0) &&
        ((info->lobin == 0) || ((info->lobin + info->hibin + info->ascii) / info->lobin >= 10)))
    {
      /* Statistically speaking, there should be more than 10% "lobin"
       * chars if this is really a binary file...  */
      att->type = TYPE_TEXT;
      att->subtype = mutt_str_dup("plain");
    }
    else
    {
      att->type = TYPE_APPLICATION;
      att->subtype = mutt_str_dup("octet-stream");
    }
  }

  FREE(&info);
  mutt_update_encoding(att, sub);
  return att;
}

/**
 * encode_headers - RFC2047-encode a list of headers
 * @param h   String List of headers
 * @param sub Config Subset
 *
 * The strings are encoded in-place.
 */
static void encode_headers(struct ListHead *h, struct ConfigSubset *sub)
{
  char *tmp = NULL;
  char *p = NULL;
  int i;

  const struct Slist *const c_send_charset = cs_subset_slist(sub, "send_charset");

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, h, entries)
  {
    p = strchr(np->data, ':');
    if (!p)
      continue;

    i = p - np->data;
    p = mutt_str_skip_email_wsp(p + 1);
    tmp = mutt_str_dup(p);

    if (!tmp)
      continue;

    rfc2047_encode(&tmp, NULL, i + 2, c_send_charset);
    mutt_mem_realloc(&np->data, i + 2 + mutt_str_len(tmp) + 1);

    sprintf(np->data + i + 2, "%s", tmp);

    FREE(&tmp);
  }
}

/**
 * mutt_fqdn - Get the Fully-Qualified Domain Name
 * @param may_hide_host If true, hide the hostname (leaving just the domain)
 * @param sub           Config Subset
 * @retval ptr  string pointer into Hostname
 * @retval NULL Error, e.g no Hostname
 *
 * @warning Do not free the returned pointer
 */
const char *mutt_fqdn(bool may_hide_host, const struct ConfigSubset *sub)
{
  const char *const c_hostname = cs_subset_string(sub, "hostname");
  if (!c_hostname || (c_hostname[0] == '@'))
    return NULL;

  const char *p = c_hostname;

  const bool c_hidden_host = cs_subset_bool(sub, "hidden_host");
  if (may_hide_host && c_hidden_host)
  {
    p = strchr(c_hostname, '.');
    if (p)
      p++;

    // sanity check: don't hide the host if the fqdn is something like example.com
    if (!p || !strchr(p, '.'))
      p = c_hostname;
  }

  return p;
}

/**
 * gen_msgid - Generate a random Message ID
 * @retval ptr Message ID
 *
 * The length of the message id is chosen such that it is maximal and fits in
 * the recommended 78 character line length for the headers Message-ID:,
 * References:, and In-Reply-To:, this leads to 62 available characters
 * (excluding `@` and `>`).  Since we choose from 32 letters, we have 32^62
 * = 2^310 different message ids.
 *
 * Examples:
 * ```
 * Message-ID: <12345678901111111111222222222233333333334444444444@123456789011>
 * In-Reply-To: <12345678901111111111222222222233333333334444444444@123456789011>
 * References: <12345678901111111111222222222233333333334444444444@123456789011>
 * ```
 *
 * The distribution of the characters to left-of-@ and right-of-@ was arbitrary.
 * The choice was made to put more into the left-id and shorten the right-id to
 * slightly mimic a common length domain name.
 *
 * @note The caller should free the string
 */
static char *gen_msgid(void)
{
  const int ID_LEFT_LEN = 50;
  const int ID_RIGHT_LEN = 12;
  char rnd_id_left[ID_LEFT_LEN + 1];
  char rnd_id_right[ID_RIGHT_LEN + 1];
  char buf[128] = { 0 };

  mutt_rand_base32(rnd_id_left, sizeof(rnd_id_left) - 1);
  mutt_rand_base32(rnd_id_right, sizeof(rnd_id_right) - 1);
  rnd_id_left[ID_LEFT_LEN] = 0;
  rnd_id_right[ID_RIGHT_LEN] = 0;

  snprintf(buf, sizeof(buf), "<%s@%s>", rnd_id_left, rnd_id_right);
  return mutt_str_dup(buf);
}

/**
 * mutt_prepare_envelope - Prepare an email header
 * @param env   Envelope to prepare
 * @param final true if this email is going to be sent (not postponed)
 * @param sub   Config Subset
 *
 * Encode all the headers prior to sending the email.
 *
 * For postponing (!final) do the necessary encodings only
 */
void mutt_prepare_envelope(struct Envelope *env, bool final, struct ConfigSubset *sub)
{
  if (final)
  {
    if (!TAILQ_EMPTY(&env->bcc) && TAILQ_EMPTY(&env->to) && TAILQ_EMPTY(&env->cc))
    {
      /* some MTA's will put an Apparently-To: header field showing the Bcc:
       * recipients if there is no To: or Cc: field, so attempt to suppress
       * it by using an empty To: field.  */
      struct Address *to = mutt_addr_new();
      to->group = true;
      mutt_addrlist_append(&env->to, to);
      mutt_addrlist_append(&env->to, mutt_addr_new());

      char buf[1024] = { 0 };
      buf[0] = '\0';
      mutt_addr_cat(buf, sizeof(buf), "undisclosed-recipients", AddressSpecials);

      to->mailbox = mutt_str_dup(buf);
    }

    mutt_set_followup_to(env, sub);

    if (!env->message_id)
      env->message_id = gen_msgid();
  }

  /* Take care of 8-bit => 7-bit conversion. */
  rfc2047_encode_envelope(env);
  encode_headers(&env->userhdrs, sub);
}

/**
 * mutt_unprepare_envelope - Undo the encodings of mutt_prepare_envelope()
 * @param env Envelope to unprepare
 *
 * Decode all the headers of an email, e.g. when the sending failed or was
 * aborted.
 */
void mutt_unprepare_envelope(struct Envelope *env)
{
  struct ListNode *item = NULL;
  STAILQ_FOREACH(item, &env->userhdrs, entries)
  {
    rfc2047_decode(&item->data);
  }

  mutt_addrlist_clear(&env->mail_followup_to);

  /* back conversions */
  rfc2047_decode_envelope(env);
}

/**
 * bounce_message - Bounce an email message
 * @param fp          Handle of message
 * @param m           Mailbox
 * @param e           Email
 * @param to          Address to bounce to
 * @param resent_from Address of new sender
 * @param env_from    Envelope of original sender
 * @param sub         Config Subset
 * @retval  0 Success
 * @retval -1 Failure
 */
static int bounce_message(FILE *fp, struct Mailbox *m, struct Email *e,
                          struct AddressList *to, const char *resent_from,
                          struct AddressList *env_from, struct ConfigSubset *sub)
{
  if (!e)
    return -1;

  int rc = 0;

  struct Buffer *tempfile = mutt_buffer_pool_get();
  mutt_buffer_mktemp(tempfile);
  FILE *fp_tmp = mutt_file_fopen(mutt_buffer_string(tempfile), "w");
  if (fp_tmp)
  {
    CopyHeaderFlags chflags = CH_XMIT | CH_NONEWLINE | CH_NOQFROM;

    const bool c_bounce_delivered = cs_subset_bool(sub, "bounce_delivered");
    if (!c_bounce_delivered)
      chflags |= CH_WEED_DELIVERED;

    if (!mutt_file_seek(fp, e->offset, SEEK_SET))
    {
      (void) mutt_file_fclose(&fp_tmp);
      return -1;
    }
    fprintf(fp_tmp, "Resent-From: %s\n", resent_from);

    struct Buffer *date = mutt_buffer_pool_get();
    mutt_date_make_date(date, cs_subset_bool(sub, "local_date_header"));
    fprintf(fp_tmp, "Resent-Date: %s\n", mutt_buffer_string(date));
    mutt_buffer_pool_release(&date);

    char *msgid_str = gen_msgid();
    fprintf(fp_tmp, "Resent-Message-ID: %s\n", msgid_str);
    FREE(&msgid_str);
    mutt_addrlist_write_file(to, fp_tmp, "Resent-To");
    mutt_copy_header(fp, e, fp_tmp, chflags, NULL, 0);
    fputc('\n', fp_tmp);
    mutt_file_copy_bytes(fp, fp_tmp, e->body->length);
    if (mutt_file_fclose(&fp_tmp) != 0)
    {
      mutt_perror(mutt_buffer_string(tempfile));
      unlink(mutt_buffer_string(tempfile));
      return -1;
    }
#ifdef USE_SMTP
    const char *const c_smtp_url = cs_subset_string(sub, "smtp_url");
    if (c_smtp_url)
    {
      rc = mutt_smtp_send(env_from, to, NULL, NULL, mutt_buffer_string(tempfile),
                          (e->body->encoding == ENC_8BIT), sub);
    }
    else
#endif
    {
      rc = mutt_invoke_sendmail(m, env_from, to, NULL, NULL, mutt_buffer_string(tempfile),
                                (e->body->encoding == ENC_8BIT), sub);
    }
  }

  mutt_buffer_pool_release(&tempfile);
  return rc;
}

/**
 * mutt_bounce_message - Bounce an email message
 * @param fp  Handle of message
 * @param m   Mailbox
 * @param e   Email
 * @param to  AddressList to bounce to
 * @param sub Config Subset
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_bounce_message(FILE *fp, struct Mailbox *m, struct Email *e,
                        struct AddressList *to, struct ConfigSubset *sub)
{
  if (!fp || !e || !to || TAILQ_EMPTY(to))
    return -1;

  const char *fqdn = mutt_fqdn(true, sub);
  char *err = NULL;

  struct Address *from = mutt_default_from(sub);
  struct AddressList from_list = TAILQ_HEAD_INITIALIZER(from_list);
  mutt_addrlist_append(&from_list, from);

  /* mutt_default_from() does not use $real_name if the real name is not set
   * in $from, so we add it here.  The reason it is not added in
   * mutt_default_from() is that during normal sending, we execute
   * send-hooks and set the real_name last so that it can be changed based
   * upon message criteria.  */
  if (!from->personal)
  {
    const char *const c_real_name = cs_subset_string(sub, "real_name");
    from->personal = mutt_str_dup(c_real_name);
  }

  mutt_addrlist_qualify(&from_list, fqdn);

  rfc2047_encode_addrlist(&from_list, "Resent-From");
  if (mutt_addrlist_to_intl(&from_list, &err))
  {
    mutt_error(_("Bad IDN %s while preparing resent-from"), err);
    FREE(&err);
    mutt_addrlist_clear(&from_list);
    return -1;
  }
  struct Buffer *resent_from = mutt_buffer_pool_get();
  mutt_addrlist_write(&from_list, resent_from, false);

#ifdef USE_NNTP
  OptNewsSend = false;
#endif

  /* prepare recipient list. idna conversion appears to happen before this
   * function is called, since the user receives confirmation of the address
   * list being bounced to.  */
  struct AddressList resent_to = TAILQ_HEAD_INITIALIZER(resent_to);
  mutt_addrlist_copy(&resent_to, to, false);
  rfc2047_encode_addrlist(&resent_to, "Resent-To");
  int rc = bounce_message(fp, m, e, &resent_to, mutt_buffer_string(resent_from),
                          &from_list, sub);
  mutt_addrlist_clear(&resent_to);
  mutt_addrlist_clear(&from_list);
  mutt_buffer_pool_release(&resent_from);
  return rc;
}

/**
 * set_noconv_flags - Set/reset the "x-mutt-noconv" flag
 * @param b    Body of email
 * @param flag If true, set the flag, otherwise remove it
 */
static void set_noconv_flags(struct Body *b, bool flag)
{
  for (; b; b = b->next)
  {
    if ((b->type == TYPE_MESSAGE) || (b->type == TYPE_MULTIPART))
      set_noconv_flags(b->parts, flag);
    else if ((b->type == TYPE_TEXT) && b->noconv)
    {
      if (flag)
        mutt_param_set(&b->parameter, "x-mutt-noconv", "yes");
      else
        mutt_param_delete(&b->parameter, "x-mutt-noconv");
    }
  }
}

/**
 * mutt_write_multiple_fcc - Handle FCC with multiple, comma separated entries
 * @param[in]  path      Path to mailboxes (comma separated)
 * @param[in]  e         Email
 * @param[in]  msgid     Message id
 * @param[in]  post      If true, postpone message
 * @param[in]  fcc       fcc setting to save (postpone only)
 * @param[out] finalpath Final path of email
 * @param[in]  sub       Config Subset
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_write_multiple_fcc(const char *path, struct Email *e, const char *msgid, bool post,
                            char *fcc, char **finalpath, struct ConfigSubset *sub)
{
  char fcc_tok[PATH_MAX] = { 0 };
  char fcc_expanded[PATH_MAX] = { 0 };

  mutt_str_copy(fcc_tok, path, sizeof(fcc_tok));

  char *tok = strtok(fcc_tok, ",");
  if (!tok)
    return -1;

  mutt_debug(LL_DEBUG1, "Fcc: initial mailbox = '%s'\n", tok);
  /* mutt_expand_path already called above for the first token */
  int status = mutt_write_fcc(tok, e, msgid, post, fcc, finalpath, sub);
  if (status != 0)
    return status;

  while ((tok = strtok(NULL, ",")))
  {
    if (*tok == '\0')
      continue;

    /* Only call mutt_expand_path if tok has some data */
    mutt_debug(LL_DEBUG1, "Fcc: additional mailbox token = '%s'\n", tok);
    mutt_str_copy(fcc_expanded, tok, sizeof(fcc_expanded));
    mutt_expand_path(fcc_expanded, sizeof(fcc_expanded));
    mutt_debug(LL_DEBUG1, "     Additional mailbox expanded = '%s'\n", fcc_expanded);
    status = mutt_write_fcc(fcc_expanded, e, msgid, post, fcc, finalpath, sub);
    if (status != 0)
      return status;
  }

  return 0;
}

/**
 * mutt_write_fcc - Write email to FCC mailbox
 * @param[in]  path      Path to mailbox
 * @param[in]  e         Email
 * @param[in]  msgid     Message id
 * @param[in]  post      If true, postpone message, else fcc mode
 * @param[in]  fcc       fcc setting to save (postpone only)
 * @param[out] finalpath Final path of email
 * @param[in]  sub       Config Subset
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_write_fcc(const char *path, struct Email *e, const char *msgid, bool post,
                   const char *fcc, char **finalpath, struct ConfigSubset *sub)
{
  struct Message *msg = NULL;
  struct Buffer *tempfile = NULL;
  FILE *fp_tmp = NULL;
  int rc = -1;
  bool need_mailbox_cleanup = false;
  struct stat st = { 0 };
  MsgOpenFlags onm_flags;

  if (post)
    set_noconv_flags(e->body, true);

#ifdef RECORD_FOLDER_HOOK
  mutt_folder_hook(path, NULL);
#endif
  struct Mailbox *m_fcc = mx_path_resolve(path);
  bool old_append = m_fcc->append;
  if (!mx_mbox_open(m_fcc, MUTT_APPEND | MUTT_QUIET))
  {
    mutt_debug(LL_DEBUG1, "unable to open mailbox %s in append-mode, aborting\n", path);
    goto done;
  }

  /* We need to add a Content-Length field to avoid problems where a line in
   * the message body begins with "From " */
  if ((m_fcc->type == MUTT_MMDF) || (m_fcc->type == MUTT_MBOX))
  {
    tempfile = mutt_buffer_pool_get();
    mutt_buffer_mktemp(tempfile);
    fp_tmp = mutt_file_fopen(mutt_buffer_string(tempfile), "w+");
    if (!fp_tmp)
    {
      mutt_perror(mutt_buffer_string(tempfile));
      mx_mbox_close(m_fcc);
      goto done;
    }
    /* remember new mail status before appending message */
    need_mailbox_cleanup = true;
    stat(path, &st);
  }

  e->read = !post; /* make sure to put it in the 'cur' directory (maildir) */
  onm_flags = MUTT_ADD_FROM;
  if (post)
    onm_flags |= MUTT_SET_DRAFT;
  msg = mx_msg_open_new(m_fcc, e, onm_flags);
  if (!msg)
  {
    mutt_file_fclose(&fp_tmp);
    mx_mbox_close(m_fcc);
    goto done;
  }

  const bool c_crypt_protected_headers_read = cs_subset_bool(sub, "crypt_protected_headers_read");

  /* post == 1 => postpone message.
   * post == 0 => Normal mode.  */
  mutt_rfc822_write_header(msg->fp, e->env, e->body,
                           post ? MUTT_WRITE_HEADER_POSTPONE : MUTT_WRITE_HEADER_FCC, false,
                           c_crypt_protected_headers_read &&
                               mutt_should_hide_protected_subject(e),
                           sub);

  /* (postponement) if this was a reply of some sort, <msgid> contains the
   * Message-ID: of message replied to.  Save it using a special X-Mutt-
   * header so it can be picked up if the message is recalled at a later
   * point in time.  This will allow the message to be marked as replied if
   * the same mailbox is still open.  */
  if (post && msgid)
    fprintf(msg->fp, "Mutt-References: %s\n", msgid);

  /* (postponement) save the Fcc: using a special X-Mutt- header so that
   * it can be picked up when the message is recalled */
  if (post && fcc)
    fprintf(msg->fp, "Mutt-Fcc: %s\n", fcc);

  if ((m_fcc->type == MUTT_MMDF) || (m_fcc->type == MUTT_MBOX))
    fprintf(msg->fp, "Status: RO\n");

  /* (postponement) if the mail is to be signed or encrypted, save this info */
  if (((WithCrypto & APPLICATION_PGP) != 0) && post && (e->security & APPLICATION_PGP))
  {
    fputs("Mutt-PGP: ", msg->fp);
    if (e->security & SEC_ENCRYPT)
      fputc('E', msg->fp);
    if (e->security & SEC_OPPENCRYPT)
      fputc('O', msg->fp);
    if (e->security & SEC_SIGN)
    {
      fputc('S', msg->fp);

      const char *const c_pgp_sign_as = cs_subset_string(sub, "pgp_sign_as");
      if (c_pgp_sign_as)
        fprintf(msg->fp, "<%s>", c_pgp_sign_as);
    }
    if (e->security & SEC_INLINE)
      fputc('I', msg->fp);
#ifdef USE_AUTOCRYPT
    if (e->security & SEC_AUTOCRYPT)
      fputc('A', msg->fp);
    if (e->security & SEC_AUTOCRYPT_OVERRIDE)
      fputc('Z', msg->fp);
#endif
    fputc('\n', msg->fp);
  }

  /* (postponement) if the mail is to be signed or encrypted, save this info */
  if (((WithCrypto & APPLICATION_SMIME) != 0) && post && (e->security & APPLICATION_SMIME))
  {
    fputs("Mutt-SMIME: ", msg->fp);
    if (e->security & SEC_ENCRYPT)
    {
      fputc('E', msg->fp);

      const char *const c_smime_encrypt_with = cs_subset_string(sub, "smime_encrypt_with");
      if (c_smime_encrypt_with)
        fprintf(msg->fp, "C<%s>", c_smime_encrypt_with);
    }
    if (e->security & SEC_OPPENCRYPT)
      fputc('O', msg->fp);
    if (e->security & SEC_SIGN)
    {
      fputc('S', msg->fp);

      const char *const c_smime_sign_as = cs_subset_string(sub, "smime_sign_as");
      if (c_smime_sign_as)
        fprintf(msg->fp, "<%s>", c_smime_sign_as);
    }
    if (e->security & SEC_INLINE)
      fputc('I', msg->fp);
    fputc('\n', msg->fp);
  }

#ifdef MIXMASTER
  /* (postponement) if the mail is to be sent through a mixmaster
   * chain, save that information */

  if (post && !STAILQ_EMPTY(&e->chain))
  {
    fputs("Mutt-Mix:", msg->fp);
    struct ListNode *p = NULL;
    STAILQ_FOREACH(p, &e->chain, entries)
    {
      fprintf(msg->fp, " %s", (char *) p->data);
    }

    fputc('\n', msg->fp);
  }
#endif

  if (fp_tmp)
  {
    mutt_write_mime_body(e->body, fp_tmp, sub);

    /* make sure the last line ends with a newline.  Emacs doesn't ensure this
     * will happen, and it can cause problems parsing the mailbox later.  */
    if (mutt_file_seek(fp_tmp, -1, SEEK_END) && (fgetc(fp_tmp) != '\n') &&
        mutt_file_seek(fp_tmp, 0, SEEK_END))
    {
      fputc('\n', fp_tmp);
    }

    fflush(fp_tmp);
    if (ferror(fp_tmp))
    {
      mutt_debug(LL_DEBUG1, "%s: write failed\n", mutt_buffer_string(tempfile));
      mutt_file_fclose(&fp_tmp);
      unlink(mutt_buffer_string(tempfile));
      mx_msg_commit(m_fcc, msg); /* XXX really? */
      mx_msg_close(m_fcc, &msg);
      mx_mbox_close(m_fcc);
      goto done;
    }

    /* count the number of lines */
    int lines = 0;
    char line_buf[1024] = { 0 };
    rewind(fp_tmp);
    while (fgets(line_buf, sizeof(line_buf), fp_tmp))
      lines++;
    fprintf(msg->fp, "Content-Length: " OFF_T_FMT "\n", (LOFF_T) ftello(fp_tmp));
    fprintf(msg->fp, "Lines: %d\n\n", lines);

    /* copy the body and clean up */
    rewind(fp_tmp);
    rc = mutt_file_copy_stream(fp_tmp, msg->fp);
    if (mutt_file_fclose(&fp_tmp) != 0)
      rc = -1;
    /* if there was an error, leave the temp version */
    if (rc >= 0)
    {
      unlink(mutt_buffer_string(tempfile));
      rc = 0;
    }
  }
  else
  {
    fputc('\n', msg->fp); /* finish off the header */
    rc = mutt_write_mime_body(e->body, msg->fp, sub);
  }

  if (mx_msg_commit(m_fcc, msg) != 0)
    rc = -1;
  else if (finalpath)
    *finalpath = mutt_str_dup(msg->committed_path);
  mx_msg_close(m_fcc, &msg);
  mx_mbox_close(m_fcc);

  if (!post && need_mailbox_cleanup)
    mutt_mailbox_cleanup(path, &st);

  if (post)
    set_noconv_flags(e->body, false);

done:
  m_fcc->append = old_append;
  mailbox_free(&m_fcc);

#ifdef RECORD_FOLDER_HOOK
  /* We ran a folder hook for the destination mailbox,
   * now we run it for the user's current mailbox */
  const struct Mailbox *m_cur = get_current_mailbox();
  if (m_cur)
    mutt_folder_hook(m_cur->path, m_cur->desc);
#endif

  if (fp_tmp)
  {
    mutt_file_fclose(&fp_tmp);
    unlink(mutt_buffer_string(tempfile));
  }
  mutt_buffer_pool_release(&tempfile);

  return rc;
}
