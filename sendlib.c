/**
 * @file
 * Miscellaneous functions for sending an email
 *
 * @authors
 * Copyright (C) 1996-2002,2009-2012 Michael R. Elkins <me@mutt.org>
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
 * @page sendlib Miscellaneous functions for sending an email
 *
 * Miscellaneous functions for sending an email
 */

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <iconv.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "address/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "sendlib.h"
#include "context.h"
#include "copy.h"
#include "curs_lib.h"
#include "filter.h"
#include "format_flags.h"
#include "globals.h"
#include "handler.h"
#include "mutt_mailbox.h"
#include "mutt_parse.h"
#include "mutt_window.h"
#include "muttlib.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "options.h"
#include "pager.h"
#include "send.h"
#include "smtp.h"
#include "state.h"
#ifdef USE_NNTP
#include "nntp/nntp.h"
#endif
#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#else
#define EX_OK 0
#endif
#ifdef USE_AUTOCRYPT
#include "autocrypt/autocrypt.h"
#endif

/* These Config Variables are only used in sendlib.c */
bool C_Allow8bit; ///< Config: Allow 8-bit messages, don't use quoted-printable or base64
char *C_AttachCharset; ///< Config: When attaching files, use one of these character sets
bool C_BounceDelivered; ///< Config: Add 'Delivered-To' to bounced messages
bool C_EncodeFrom; ///< Config: Encode 'From ' as 'quote-printable' at the beginning of lines
bool C_ForwardDecrypt; ///< Config: Decrypt the message when forwarding it
bool C_HiddenHost; ///< Config: Don't use the hostname, just the domain, when generating the message id
char *C_Inews;     ///< Config: (nntp) External command to post news articles
bool C_MimeForwardDecode; ///< Config: Decode the forwarded message before attaching it
bool C_MimeSubject; ///< Config: (nntp) Encode the article subject in base64
char *C_MimeTypeQueryCommand; ///< Config: External command to determine the MIME type of an attachment
bool C_MimeTypeQueryFirst; ///< Config: Run the #C_MimeTypeQueryCommand before the mime.types lookup
char *C_Sendmail;     ///< Config: External command to send email
short C_SendmailWait; ///< Config: Time to wait for sendmail to finish
bool C_Use8bitmime;   ///< Config: Use 8-bit messages and ESMTP to send messages
bool C_UseEnvelopeFrom; ///< Config: Set the envelope sender of the message
bool C_UserAgent;       ///< Config: Add a 'User-Agent' head to outgoing mail
short C_WrapHeaders;    ///< Config: Width to wrap headers in outgoing messages

/**
 * encode_quoted - Encode text as quoted printable
 * @param fc     Cursor for converting a file's encoding
 * @param fp_out File to store the result
 * @param istext Is the input text?
 */
static void encode_quoted(struct FgetConv *fc, FILE *fp_out, bool istext)
{
  int c, linelen = 0;
  char line[77], savechar;

  while ((c = mutt_ch_fgetconv(fc)) != EOF)
  {
    /* Wrap the line if needed. */
    if ((linelen == 76) && ((istext && (c != '\n')) || !istext))
    {
      /* If the last character is "quoted", then be sure to move all three
       * characters to the next line.  Otherwise, just move the last
       * character...  */
      if (line[linelen - 3] == '=')
      {
        line[linelen - 3] = 0;
        fputs(line, fp_out);
        fputs("=\n", fp_out);
        line[linelen] = 0;
        line[0] = '=';
        line[1] = line[linelen - 2];
        line[2] = line[linelen - 1];
        linelen = 3;
      }
      else
      {
        savechar = line[linelen - 1];
        line[linelen - 1] = '=';
        line[linelen] = 0;
        fputs(line, fp_out);
        fputc('\n', fp_out);
        line[0] = savechar;
        linelen = 1;
      }
    }

    /* Escape lines that begin with/only contain "the message separator". */
    if ((linelen == 4) && mutt_str_startswith(line, "From", CASE_MATCH))
    {
      mutt_str_strfcpy(line, "=46rom", sizeof(line));
      linelen = 6;
    }
    else if ((linelen == 4) && mutt_str_startswith(line, "from", CASE_MATCH))
    {
      mutt_str_strfcpy(line, "=66rom", sizeof(line));
      linelen = 6;
    }
    else if ((linelen == 1) && (line[0] == '.'))
    {
      mutt_str_strfcpy(line, "=2E", sizeof(line));
      linelen = 3;
    }

    if ((c == '\n') && istext)
    {
      /* Check to make sure there is no trailing space on this line. */
      if ((linelen > 0) && ((line[linelen - 1] == ' ') || (line[linelen - 1] == '\t')))
      {
        if (linelen < 74)
        {
          sprintf(line + linelen - 1, "=%2.2X", (unsigned char) line[linelen - 1]);
          fputs(line, fp_out);
        }
        else
        {
          int savechar2 = line[linelen - 1];

          line[linelen - 1] = '=';
          line[linelen] = 0;
          fputs(line, fp_out);
          fprintf(fp_out, "\n=%2.2X", (unsigned char) savechar2);
        }
      }
      else
      {
        line[linelen] = 0;
        fputs(line, fp_out);
      }
      fputc('\n', fp_out);
      linelen = 0;
    }
    else if ((c != 9) && ((c < 32) || (c > 126) || (c == '=')))
    {
      /* Check to make sure there is enough room for the quoted character.
       * If not, wrap to the next line.  */
      if (linelen > 73)
      {
        line[linelen++] = '=';
        line[linelen] = 0;
        fputs(line, fp_out);
        fputc('\n', fp_out);
        linelen = 0;
      }
      sprintf(line + linelen, "=%2.2X", (unsigned char) c);
      linelen += 3;
    }
    else
    {
      /* Don't worry about wrapping the line here.  That will happen during
       * the next iteration when I'll also know what the next character is.  */
      line[linelen++] = c;
    }
  }

  /* Take care of anything left in the buffer */
  if (linelen > 0)
  {
    if ((line[linelen - 1] == ' ') || (line[linelen - 1] == '\t'))
    {
      /* take care of trailing whitespace */
      if (linelen < 74)
        sprintf(line + linelen - 1, "=%2.2X", (unsigned char) line[linelen - 1]);
      else
      {
        savechar = line[linelen - 1];
        line[linelen - 1] = '=';
        line[linelen] = 0;
        fputs(line, fp_out);
        fputc('\n', fp_out);
        sprintf(line, "=%2.2X", (unsigned char) savechar);
      }
    }
    else
      line[linelen] = 0;
    fputs(line, fp_out);
  }
}

/**
 * struct B64Context - Cursor for the Base64 conversion
 */
struct B64Context
{
  char buffer[3];
  short size;
  short linelen;
};

/**
 * b64_init - Set up the base64 conversion
 * @param bctx Cursor for the base64 conversion
 * @retval 0 Always
 */
static int b64_init(struct B64Context *bctx)
{
  memset(bctx->buffer, '\0', sizeof(bctx->buffer));
  bctx->size = 0;
  bctx->linelen = 0;

  return 0;
}

/**
 * b64_flush - Save the bytes to the file
 * @param bctx   Cursor for the base64 conversion
 * @param fp_out File to save the output
 */
static void b64_flush(struct B64Context *bctx, FILE *fp_out)
{
  /* for some reasons, mutt_b64_encode expects the
   * output buffer to be larger than 10B */
  char encoded[11];
  size_t ret;

  if (bctx->size == 0)
    return;

  if (bctx->linelen >= 72)
  {
    fputc('\n', fp_out);
    bctx->linelen = 0;
  }

  /* ret should always be equal to 4 here, because bctx->size
   * is a value between 1 and 3 (included), but let's not hardcode it
   * and prefer the return value of the function */
  ret = mutt_b64_encode(bctx->buffer, bctx->size, encoded, sizeof(encoded));
  for (size_t i = 0; i < ret; i++)
  {
    fputc(encoded[i], fp_out);
    bctx->linelen++;
  }

  bctx->size = 0;
}

/**
 * b64_putc - Base64-encode one character
 * @param bctx   Cursor for the base64 conversion
 * @param c      Character to encode
 * @param fp_out File to save the output
 */
static void b64_putc(struct B64Context *bctx, char c, FILE *fp_out)
{
  if (bctx->size == 3)
    b64_flush(bctx, fp_out);

  bctx->buffer[bctx->size++] = c;
}

/**
 * encode_base64 - Base64-encode some data
 * @param fc     Cursor for converting a file's encoding
 * @param fp_out File to store the result
 * @param istext Is the input text?
 */
static void encode_base64(struct FgetConv *fc, FILE *fp_out, int istext)
{
  struct B64Context bctx;
  int ch, ch1 = EOF;

  b64_init(&bctx);

  while ((ch = mutt_ch_fgetconv(fc)) != EOF)
  {
    if (SigInt == 1)
    {
      SigInt = 0;
      return;
    }
    if (istext && (ch == '\n') && (ch1 != '\r'))
      b64_putc(&bctx, '\r', fp_out);
    b64_putc(&bctx, ch, fp_out);
    ch1 = ch;
  }
  b64_flush(&bctx, fp_out);
  fputc('\n', fp_out);
}

/**
 * encode_8bit - Write the data as raw 8-bit data
 * @param fc     Cursor for converting a file's encoding
 * @param fp_out File to store the result
 */
static void encode_8bit(struct FgetConv *fc, FILE *fp_out)
{
  int ch;

  while ((ch = mutt_ch_fgetconv(fc)) != EOF)
  {
    if (SigInt == 1)
    {
      SigInt = 0;
      return;
    }
    fputc(ch, fp_out);
  }
}

/**
 * mutt_write_mime_header - Create a MIME header
 * @param a  Body part
 * @param fp File to write to
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_write_mime_header(struct Body *a, FILE *fp)
{
  if (!a || !fp)
    return -1;

  int len;
  int tmplen;
  char buf[256] = { 0 };

  fprintf(fp, "Content-Type: %s/%s", TYPE(a), a->subtype);

  if (!TAILQ_EMPTY(&a->parameter))
  {
    len = 25 + mutt_str_strlen(a->subtype); /* approximate len. of content-type */

    struct Parameter *np = NULL;
    TAILQ_FOREACH(np, &a->parameter, entries)
    {
      if (!np->attribute || !np->value)
        continue;

      struct ParameterList pl_conts = rfc2231_encode_string(np->attribute, np->value);
      struct Parameter *cont = NULL;
      TAILQ_FOREACH(cont, &pl_conts, entries)
      {
        fputc(';', fp);

        buf[0] = 0;
        mutt_addr_cat(buf, sizeof(buf), cont->value, MimeSpecials);

        /* Dirty hack to make messages readable by Outlook Express
         * for the Mac: force quotes around the boundary parameter
         * even when they aren't needed.
         */
        if (!mutt_str_strcasecmp(cont->attribute, "boundary") &&
            !mutt_str_strcmp(buf, cont->value))
          snprintf(buf, sizeof(buf), "\"%s\"", cont->value);

        tmplen = mutt_str_strlen(buf) + mutt_str_strlen(cont->attribute) + 1;
        if (len + tmplen + 2 > 76)
        {
          fputs("\n\t", fp);
          len = tmplen + 1;
        }
        else
        {
          fputc(' ', fp);
          len += tmplen + 1;
        }

        fprintf(fp, "%s=%s", cont->attribute, buf);
      }

      mutt_param_free(&pl_conts);
    }
  }

  fputc('\n', fp);

  if (a->language)
    fprintf(fp, "Content-Language: %s\n", a->language);

  if (a->description)
    fprintf(fp, "Content-Description: %s\n", a->description);

  if (a->disposition != DISP_NONE)
  {
    const char *dispstr[] = { "inline", "attachment", "form-data" };

    if (a->disposition < sizeof(dispstr) / sizeof(char *))
    {
      fprintf(fp, "Content-Disposition: %s", dispstr[a->disposition]);
      len = 21 + mutt_str_strlen(dispstr[a->disposition]);

      if (a->use_disp && (a->disposition != DISP_INLINE))
      {
        char *fn = a->d_filename;
        if (!fn)
          fn = a->filename;

        if (fn)
        {
          /* Strip off the leading path... */
          char *t = strrchr(fn, '/');
          if (t)
            t++;
          else
            t = fn;

          struct ParameterList pl_conts = rfc2231_encode_string("filename", t);
          struct Parameter *cont = NULL;
          TAILQ_FOREACH(cont, &pl_conts, entries)
          {
            fputc(';', fp);
            buf[0] = 0;
            mutt_addr_cat(buf, sizeof(buf), cont->value, MimeSpecials);

            tmplen = mutt_str_strlen(buf) + mutt_str_strlen(cont->attribute) + 1;
            if (len + tmplen + 2 > 76)
            {
              fputs("\n\t", fp);
              len = tmplen + 1;
            }
            else
            {
              fputc(' ', fp);
              len += tmplen + 1;
            }

            fprintf(fp, "%s=%s", cont->attribute, buf);
          }

          mutt_param_free(&pl_conts);
        }
      }

      fputc('\n', fp);
    }
    else
    {
      mutt_debug(LL_DEBUG1, "ERROR: invalid content-disposition %d\n", a->disposition);
    }
  }

  if (a->encoding != ENC_7BIT)
    fprintf(fp, "Content-Transfer-Encoding: %s\n", ENCODING(a->encoding));

  if ((C_CryptProtectedHeadersWrite
#ifdef USE_AUTOCRYPT
       || C_Autocrypt
#endif
       ) &&
      a->mime_headers)
  {
    mutt_rfc822_write_header(fp, a->mime_headers, NULL, MUTT_WRITE_HEADER_MIME, false, false);
  }

  /* Do NOT add the terminator here!!! */
  return ferror(fp) ? -1 : 0;
}

/**
 * write_as_text_part - Should the Body be written as a text MIME part
 * @param b Email to examine
 * @retval true If the Body should be written as text
 */
static bool write_as_text_part(struct Body *b)
{
  return mutt_is_text_part(b) ||
         (((WithCrypto & APPLICATION_PGP) != 0) && mutt_is_application_pgp(b));
}

/**
 * mutt_write_mime_body - Write a MIME part
 * @param a  Body to use
 * @param fp File to write to
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_write_mime_body(struct Body *a, FILE *fp)
{
  FILE *fp_in = NULL;
  struct FgetConv *fc = NULL;

  if (a->type == TYPE_MULTIPART)
  {
    /* First, find the boundary to use */
    const char *p = mutt_param_get(&a->parameter, "boundary");
    if (!p)
    {
      mutt_debug(LL_DEBUG1, "no boundary parameter found\n");
      mutt_error(_("No boundary parameter found [report this error]"));
      return -1;
    }
    char boundary[128];
    mutt_str_strfcpy(boundary, p, sizeof(boundary));

    for (struct Body *t = a->parts; t; t = t->next)
    {
      fprintf(fp, "\n--%s\n", boundary);
      if (mutt_write_mime_header(t, fp) == -1)
        return -1;
      fputc('\n', fp);
      if (mutt_write_mime_body(t, fp) == -1)
        return -1;
    }
    fprintf(fp, "\n--%s--\n", boundary);
    return ferror(fp) ? -1 : 0;
  }

  /* This is pretty gross, but it's the best solution for now... */
  if (((WithCrypto & APPLICATION_PGP) != 0) && (a->type == TYPE_APPLICATION) &&
      (mutt_str_strcmp(a->subtype, "pgp-encrypted") == 0))
  {
    fputs("Version: 1\n", fp);
    return 0;
  }

  fp_in = fopen(a->filename, "r");
  if (!fp_in)
  {
    mutt_debug(LL_DEBUG1, "%s no longer exists\n", a->filename);
    mutt_error(_("%s no longer exists"), a->filename);
    return -1;
  }

  if ((a->type == TYPE_TEXT) && (!a->noconv))
  {
    char send_charset[128];
    fc = mutt_ch_fgetconv_open(
        fp_in, a->charset,
        mutt_body_get_charset(a, send_charset, sizeof(send_charset)), 0);
  }
  else
    fc = mutt_ch_fgetconv_open(fp_in, 0, 0, 0);

  mutt_sig_allow_interrupt(1);
  if (a->encoding == ENC_QUOTED_PRINTABLE)
    encode_quoted(fc, fp, write_as_text_part(a));
  else if (a->encoding == ENC_BASE64)
    encode_base64(fc, fp, write_as_text_part(a));
  else if ((a->type == TYPE_TEXT) && (!a->noconv))
    encode_8bit(fc, fp);
  else
    mutt_file_copy_stream(fp_in, fp);
  mutt_sig_allow_interrupt(0);

  mutt_ch_fgetconv_close(&fc);
  mutt_file_fclose(&fp_in);

  if (SigInt == 1)
  {
    SigInt = 0;
    return -1;
  }
  return ferror(fp) ? -1 : 0;
}

/**
 * mutt_generate_boundary - Create a unique boundary id for a MIME part
 * @param pl MIME part
 */
void mutt_generate_boundary(struct ParameterList *pl)
{
  char rs[MUTT_RANDTAG_LEN + 1];

  mutt_rand_base32(rs, sizeof(rs) - 1);
  rs[MUTT_RANDTAG_LEN] = 0;
  mutt_param_set(pl, "boundary", rs);
}

/**
 * struct ContentState - Info about the body of an email
 */
struct ContentState
{
  bool from;
  int whitespace;
  bool dot;
  int linelen;
  bool was_cr;
};

/**
 * update_content_info - Cache some info about an email
 * @param info   Info about an Attachment
 * @param s      Info about the Body of an email
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 */
static void update_content_info(struct Content *info, struct ContentState *s,
                                char *buf, size_t buflen)
{
  bool from = s->from;
  int whitespace = s->whitespace;
  bool dot = s->dot;
  int linelen = s->linelen;
  bool was_cr = s->was_cr;

  if (!buf) /* This signals EOF */
  {
    if (was_cr)
      info->binary = true;
    if (linelen > info->linemax)
      info->linemax = linelen;

    return;
  }

  for (; buflen; buf++, buflen--)
  {
    char ch = *buf;

    if (was_cr)
    {
      was_cr = false;
      if (ch != '\n')
      {
        info->binary = true;
      }
      else
      {
        if (whitespace)
          info->space = true;
        if (dot)
          info->dot = true;
        if (linelen > info->linemax)
          info->linemax = linelen;
        whitespace = 0;
        dot = false;
        linelen = 0;
        continue;
      }
    }

    linelen++;
    if (ch == '\n')
    {
      info->crlf++;
      if (whitespace)
        info->space = true;
      if (dot)
        info->dot = true;
      if (linelen > info->linemax)
        info->linemax = linelen;
      whitespace = 0;
      linelen = 0;
      dot = false;
    }
    else if (ch == '\r')
    {
      info->crlf++;
      info->cr = true;
      was_cr = true;
      continue;
    }
    else if (ch & 0x80)
      info->hibin++;
    else if ((ch == '\t') || (ch == '\f'))
    {
      info->ascii++;
      whitespace++;
    }
    else if (ch == 0)
    {
      info->nulbin++;
      info->lobin++;
    }
    else if ((ch < 32) || (ch == 127))
      info->lobin++;
    else
    {
      if (linelen == 1)
      {
        if ((ch == 'F') || (ch == 'f'))
          from = true;
        else
          from = false;
        if (ch == '.')
          dot = true;
        else
          dot = false;
      }
      else if (from)
      {
        if ((linelen == 2) && (ch != 'r'))
          from = false;
        else if ((linelen == 3) && (ch != 'o'))
          from = false;
        else if (linelen == 4)
        {
          if (ch == 'm')
            info->from = true;
          from = false;
        }
      }
      if (ch == ' ')
        whitespace++;
      info->ascii++;
    }

    if (linelen > 1)
      dot = false;
    if ((ch != ' ') && (ch != '\t'))
      whitespace = 0;
  }

  s->from = from;
  s->whitespace = whitespace;
  s->dot = dot;
  s->linelen = linelen;
  s->was_cr = was_cr;
}

/**
 * convert_file_to - Change the encoding of a file
 * @param[in]  fp         File to convert
 * @param[in]  fromcode   Original encoding
 * @param[in]  ncodes     Number of target encodings
 * @param[in]  tocodes    List of target encodings
 * @param[out] tocode     Chosen encoding
 * @param[in]  info       Encoding information
 * @retval -1 Error, no conversion was possible
 * @retval >0 Success, number of bytes converted
 *
 * Find the best charset conversion of the file from fromcode into one
 * of the tocodes. If successful, set *tocode and Content *info and
 * return the number of characters converted inexactly.
 *
 * We convert via UTF-8 in order to avoid the condition -1(EINVAL),
 * which would otherwise prevent us from knowing the number of inexact
 * conversions. Where the candidate target charset is UTF-8 we avoid
 * doing the second conversion because iconv_open("UTF-8", "UTF-8")
 * fails with some libraries.
 *
 * We assume that the output from iconv is never more than 4 times as
 * long as the input for any pair of charsets we might be interested
 * in.
 */
static size_t convert_file_to(FILE *fp, const char *fromcode, int ncodes,
                              char const *const *tocodes, int *tocode, struct Content *info)
{
  char bufi[256], bufu[512], bufo[4 * sizeof(bufi)];
  size_t ret;

  const iconv_t cd1 = mutt_ch_iconv_open("utf-8", fromcode, 0);
  if (cd1 == (iconv_t)(-1))
    return -1;

  iconv_t *cd = mutt_mem_calloc(ncodes, sizeof(iconv_t));
  size_t *score = mutt_mem_calloc(ncodes, sizeof(size_t));
  struct ContentState *states = mutt_mem_calloc(ncodes, sizeof(struct ContentState));
  struct Content *infos = mutt_mem_calloc(ncodes, sizeof(struct Content));

  for (int i = 0; i < ncodes; i++)
  {
    if (mutt_str_strcasecmp(tocodes[i], "utf-8") != 0)
      cd[i] = mutt_ch_iconv_open(tocodes[i], "utf-8", 0);
    else
    {
      /* Special case for conversion to UTF-8 */
      cd[i] = (iconv_t)(-1);
      score[i] = (size_t)(-1);
    }
  }

  rewind(fp);
  size_t ibl = 0;
  while (true)
  {
    /* Try to fill input buffer */
    size_t n = fread(bufi + ibl, 1, sizeof(bufi) - ibl, fp);
    ibl += n;

    /* Convert to UTF-8 */
    const char *ib = bufi;
    char *ob = bufu;
    size_t obl = sizeof(bufu);
    n = iconv(cd1, (ICONV_CONST char **) ((ibl != 0) ? &ib : 0), &ibl, &ob, &obl);
    /* assert(n == (size_t)(-1) || !n); */
    if ((n == (size_t)(-1)) && (((errno != EINVAL) && (errno != E2BIG)) || (ib == bufi)))
    {
      /* assert(errno == EILSEQ || (errno == EINVAL && ib == bufi && ibl < sizeof(bufi))); */
      ret = (size_t)(-1);
      break;
    }
    const size_t ubl1 = ob - bufu;

    /* Convert from UTF-8 */
    for (int i = 0; i < ncodes; i++)
    {
      if ((cd[i] != (iconv_t)(-1)) && (score[i] != (size_t)(-1)))
      {
        const char *ub = bufu;
        size_t ubl = ubl1;
        ob = bufo;
        obl = sizeof(bufo);
        n = iconv(cd[i], (ICONV_CONST char **) ((ibl || ubl) ? &ub : 0), &ubl, &ob, &obl);
        if (n == (size_t)(-1))
        {
          /* assert(errno == E2BIG || (BUGGY_ICONV && (errno == EILSEQ || errno == ENOENT))); */
          score[i] = (size_t)(-1);
        }
        else
        {
          score[i] += n;
          update_content_info(&infos[i], &states[i], bufo, ob - bufo);
        }
      }
      else if ((cd[i] == (iconv_t)(-1)) && (score[i] == (size_t)(-1)))
      {
        /* Special case for conversion to UTF-8 */
        update_content_info(&infos[i], &states[i], bufu, ubl1);
      }
    }

    if (ibl)
    {
      /* Save unused input */
      memmove(bufi, ib, ibl);
    }
    else if (!ubl1 && (ib < bufi + sizeof(bufi)))
    {
      ret = 0;
      break;
    }
  }

  if (ret == 0)
  {
    /* Find best score */
    ret = (size_t)(-1);
    for (int i = 0; i < ncodes; i++)
    {
      if ((cd[i] == (iconv_t)(-1)) && (score[i] == (size_t)(-1)))
      {
        /* Special case for conversion to UTF-8 */
        *tocode = i;
        ret = 0;
        break;
      }
      else if ((cd[i] == (iconv_t)(-1)) || (score[i] == (size_t)(-1)))
        continue;
      else if ((ret == (size_t)(-1)) || (score[i] < ret))
      {
        *tocode = i;
        ret = score[i];
        if (ret == 0)
          break;
      }
    }
    if (ret != (size_t)(-1))
    {
      memcpy(info, &infos[*tocode], sizeof(struct Content));
      update_content_info(info, &states[*tocode], 0, 0); /* EOF */
    }
  }

  for (int i = 0; i < ncodes; i++)
    if (cd[i] != (iconv_t)(-1))
      iconv_close(cd[i]);

  iconv_close(cd1);
  FREE(&cd);
  FREE(&infos);
  FREE(&score);
  FREE(&states);

  return ret;
}

/**
 * convert_file_from_to - Convert a file between encodings
 * @param[in]  fp        File to read from
 * @param[in]  fromcodes Charsets to try converting FROM
 * @param[in]  tocodes   Charsets to try converting TO
 * @param[out] fromcode  From charset selected
 * @param[out] tocode    To charset selected
 * @param[out] info      Info about the file
 * @retval num Characters converted
 * @retval -1  Error (as a size_t)
 *
 * Find the first of the fromcodes that gives a valid conversion and the best
 * charset conversion of the file into one of the tocodes. If successful, set
 * *fromcode and *tocode to dynamically allocated strings, set Content *info,
 * and return the number of characters converted inexactly. If no conversion
 * was possible, return -1.
 *
 * Both fromcodes and tocodes may be colon-separated lists of charsets.
 * However, if fromcode is zero then fromcodes is assumed to be the name of a
 * single charset even if it contains a colon.
 */
static size_t convert_file_from_to(FILE *fp, const char *fromcodes, const char *tocodes,
                                   char **fromcode, char **tocode, struct Content *info)
{
  char *fcode = NULL;
  char **tcode = NULL;
  const char *c = NULL, *c1 = NULL;
  size_t ret;
  int ncodes, i, cn;

  /* Count the tocodes */
  ncodes = 0;
  for (c = tocodes; c; c = c1 ? c1 + 1 : 0)
  {
    c1 = strchr(c, ':');
    if (c1 == c)
      continue;
    ncodes++;
  }

  /* Copy them */
  tcode = mutt_mem_malloc(ncodes * sizeof(char *));
  for (c = tocodes, i = 0; c; c = c1 ? c1 + 1 : 0, i++)
  {
    c1 = strchr(c, ':');
    if (c1 == c)
      continue;
    tcode[i] = mutt_str_substr_dup(c, c1);
  }

  ret = (size_t)(-1);
  if (fromcode)
  {
    /* Try each fromcode in turn */
    for (c = fromcodes; c; c = c1 ? c1 + 1 : 0)
    {
      c1 = strchr(c, ':');
      if (c1 == c)
        continue;
      fcode = mutt_str_substr_dup(c, c1);

      ret = convert_file_to(fp, fcode, ncodes, (char const *const *) tcode, &cn, info);
      if (ret != (size_t)(-1))
      {
        *fromcode = fcode;
        *tocode = tcode[cn];
        tcode[cn] = 0;
        break;
      }
      FREE(&fcode);
    }
  }
  else
  {
    /* There is only one fromcode */
    ret = convert_file_to(fp, fromcodes, ncodes, (char const *const *) tcode, &cn, info);
    if (ret != (size_t)(-1))
    {
      *tocode = tcode[cn];
      tcode[cn] = 0;
    }
  }

  /* Free memory */
  for (i = 0; i < ncodes; i++)
    FREE(&tcode[i]);

  FREE(&tcode);

  return ret;
}

/**
 * mutt_get_content_info - Analyze file to determine MIME encoding to use
 * @param fname File to examine
 * @param b     Body to update
 * @retval ptr Newly allocated Content
 *
 * Also set the body charset, sometimes, or not.
 */
struct Content *mutt_get_content_info(const char *fname, struct Body *b)
{
  struct Content *info = NULL;
  struct ContentState state = { 0 };
  FILE *fp = NULL;
  char *fromcode = NULL;
  char *tocode = NULL;
  char buf[100];
  size_t r;

  struct stat sb;

  if (b && !fname)
    fname = b->filename;

  if (stat(fname, &sb) == -1)
  {
    mutt_error(_("Can't stat %s: %s"), fname, strerror(errno));
    return NULL;
  }

  if (!S_ISREG(sb.st_mode))
  {
    mutt_error(_("%s isn't a regular file"), fname);
    return NULL;
  }

  fp = fopen(fname, "r");
  if (!fp)
  {
    mutt_debug(LL_DEBUG1, "%s: %s (errno %d)\n", fname, strerror(errno), errno);
    return NULL;
  }

  info = mutt_mem_calloc(1, sizeof(struct Content));

  if (b && (b->type == TYPE_TEXT) && (!b->noconv && !b->force_charset))
  {
    char *chs = mutt_param_get(&b->parameter, "charset");
    char *fchs = b->use_disp ? (C_AttachCharset ? C_AttachCharset : C_Charset) : C_Charset;
    if (C_Charset && (chs || C_SendCharset) &&
        (convert_file_from_to(fp, fchs, chs ? chs : C_SendCharset, &fromcode,
                              &tocode, info) != (size_t)(-1)))
    {
      if (!chs)
      {
        char chsbuf[256];
        mutt_ch_canonical_charset(chsbuf, sizeof(chsbuf), tocode);
        mutt_param_set(&b->parameter, "charset", chsbuf);
      }
      FREE(&b->charset);
      b->charset = fromcode;
      FREE(&tocode);
      mutt_file_fclose(&fp);
      return info;
    }
  }

  rewind(fp);
  while ((r = fread(buf, 1, sizeof(buf), fp)))
    update_content_info(info, &state, buf, r);
  update_content_info(info, &state, 0, 0);

  mutt_file_fclose(&fp);

  if (b && (b->type == TYPE_TEXT) && (!b->noconv && !b->force_charset))
  {
    mutt_param_set(&b->parameter, "charset",
                   (!info->hibin ?
                        "us-ascii" :
                        C_Charset && !mutt_ch_is_us_ascii(C_Charset) ? C_Charset : "unknown-8bit"));
  }

  return info;
}

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
  char buf[PATH_MAX];
  char subtype[256] = { 0 };
  char xtype[256] = { 0 };
  int sze, cur_sze = 0;
  bool found_mimetypes = false;
  enum ContentType type = TYPE_OTHER;

  int szf = mutt_str_strlen(path);

  for (int count = 0; count < 4; count++)
  {
    /* can't use strtok() because we use it in an inner loop below, so use
     * a switch statement here instead.  */
    switch (count)
    {
      /* last file with last entry to match wins type/xtype */
      case 0:
        /* check default unix mimetypes location first */
        mutt_str_strfcpy(buf, "/etc/mime.types", sizeof(buf));
        break;
      case 1:
        mutt_str_strfcpy(buf, SYSCONFDIR "/mime.types", sizeof(buf));
        break;
      case 2:
        mutt_str_strfcpy(buf, PKGDATADIR "/mime.types", sizeof(buf));
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
          sze = mutt_str_strlen(p);
          if ((sze > cur_sze) && (szf >= sze) &&
              (mutt_str_strcasecmp(path + szf - sze, p) == 0) &&
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
              ;

            mutt_str_substr_copy(p, q, subtype, sizeof(subtype));

            type = mutt_check_mime_type(ct);
            if (type == TYPE_OTHER)
              mutt_str_strfcpy(xtype, ct, sizeof(xtype));

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
    mutt_error(_("Could not find any mime.types file."));
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
 * @param a    Body of the email
 * @param fp_in File to read
 */
static void transform_to_7bit(struct Body *a, FILE *fp_in)
{
  char buf[PATH_MAX];
  struct State s = { 0 };
  struct stat sb;

  for (; a; a = a->next)
  {
    if (a->type == TYPE_MULTIPART)
    {
      if (a->encoding != ENC_7BIT)
        a->encoding = ENC_7BIT;

      transform_to_7bit(a->parts, fp_in);
    }
    else if (mutt_is_message_type(a->type, a->subtype))
    {
      mutt_message_to_7bit(a, fp_in);
    }
    else
    {
      a->noconv = true;
      a->force_charset = true;

      mutt_mktemp(buf, sizeof(buf));
      s.fp_out = mutt_file_fopen(buf, "w");
      if (!s.fp_out)
      {
        mutt_perror("fopen");
        return;
      }
      s.fp_in = fp_in;
      mutt_decode_attachment(a, &s);
      mutt_file_fclose(&s.fp_out);
      FREE(&a->d_filename);
      a->d_filename = a->filename;
      a->filename = mutt_str_strdup(buf);
      a->unlink = true;
      if (stat(a->filename, &sb) == -1)
      {
        mutt_perror("stat");
        return;
      }
      a->length = sb.st_size;

      mutt_update_encoding(a);
      if (a->encoding == ENC_8BIT)
        a->encoding = ENC_QUOTED_PRINTABLE;
      else if (a->encoding == ENC_BINARY)
        a->encoding = ENC_BASE64;
    }
  }
}

/**
 * mutt_message_to_7bit - Convert an email's MIME parts to 7-bit
 * @param a  Body of the email
 * @param fp File to read (OPTIONAL)
 */
void mutt_message_to_7bit(struct Body *a, FILE *fp)
{
  char temp[PATH_MAX];
  char *line = NULL;
  FILE *fp_in = NULL;
  FILE *fp_out = NULL;
  struct stat sb;

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
    if (stat(a->filename, &sb) == -1)
    {
      mutt_perror("stat");
      mutt_file_fclose(&fp_in);
    }
    a->length = sb.st_size;
  }

  mutt_mktemp(temp, sizeof(temp));
  fp_out = mutt_file_fopen(temp, "w+");
  if (!fp_out)
  {
    mutt_perror("fopen");
    goto cleanup;
  }

  if (!fp_in)
    goto cleanup;

  fseeko(fp_in, a->offset, SEEK_SET);
  a->parts = mutt_rfc822_parse_message(fp_in, a);

  transform_to_7bit(a->parts, fp_in);

  mutt_copy_hdr(fp_in, fp_out, a->offset, a->offset + a->length,
                CH_MIME | CH_NONEWLINE | CH_XMIT, NULL);

  fputs("MIME-Version: 1.0\n", fp_out);
  mutt_write_mime_header(a->parts, fp_out);
  fputc('\n', fp_out);
  mutt_write_mime_body(a->parts, fp_out);

cleanup:
  FREE(&line);

  if (fp_in && (fp_in != fp))
    mutt_file_fclose(&fp_in);
  if (fp_out)
    mutt_file_fclose(&fp_out);
  else
    return;

  a->encoding = ENC_7BIT;
  FREE(&a->d_filename);
  a->d_filename = a->filename;
  if (a->filename && a->unlink)
    unlink(a->filename);
  a->filename = mutt_str_strdup(temp);
  a->unlink = true;
  if (stat(a->filename, &sb) == -1)
  {
    mutt_perror("stat");
    return;
  }
  a->length = sb.st_size;
  mutt_body_free(&a->parts);
  a->email->content = NULL;
}

/**
 * set_encoding - determine which Content-Transfer-Encoding to use
 * @param[in]  b    Body of email
 * @param[out] info Info about the email
 */
static void set_encoding(struct Body *b, struct Content *info)
{
  if (b->type == TYPE_TEXT)
  {
    char send_charset[128];
    char *chsname = mutt_body_get_charset(b, send_charset, sizeof(send_charset));
    if ((info->lobin && !mutt_str_startswith(chsname, "iso-2022", CASE_IGNORE)) ||
        (info->linemax > 990) || (info->from && C_EncodeFrom))
    {
      b->encoding = ENC_QUOTED_PRINTABLE;
    }
    else if (info->hibin)
    {
      b->encoding = C_Allow8bit ? ENC_8BIT : ENC_QUOTED_PRINTABLE;
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
      if (C_Allow8bit && !info->lobin)
        b->encoding = ENC_8BIT;
      else
        mutt_message_to_7bit(b, NULL);
    }
    else
      b->encoding = ENC_7BIT;
  }
  else if ((b->type == TYPE_APPLICATION) &&
           (mutt_str_strcasecmp(b->subtype, "pgp-keys") == 0))
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
  a->stamp = mutt_date_epoch();
}

/**
 * mutt_body_get_charset - Get a body's character set
 * @param b      Body to examine
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 * @retval ptr  Buffer containing character set
 * @retval NULL On error, or if not a text type
 */
char *mutt_body_get_charset(struct Body *b, char *buf, size_t buflen)
{
  char *p = NULL;

  if (b && (b->type != TYPE_TEXT))
    return NULL;

  if (b)
    p = mutt_param_get(&b->parameter, "charset");

  if (p)
    mutt_ch_canonical_charset(buf, buflen, p);
  else
    mutt_str_strfcpy(buf, "us-ascii", buflen);

  return buf;
}

/**
 * mutt_update_encoding - Update the encoding type
 * @param a Body to update
 *
 * Assumes called from send mode where Body->filename points to actual file
 */
void mutt_update_encoding(struct Body *a)
{
  struct Content *info = NULL;
  char chsbuf[256];

  /* override noconv when it's us-ascii */
  if (mutt_ch_is_us_ascii(mutt_body_get_charset(a, chsbuf, sizeof(chsbuf))))
    a->noconv = false;

  if (!a->force_charset && !a->noconv)
    mutt_param_delete(&a->parameter, "charset");

  info = mutt_get_content_info(a->filename, a);
  if (!info)
    return;

  set_encoding(a, info);
  mutt_stamp_attachment(a);

  FREE(&a->content);
  a->content = info;
}

/**
 * mutt_make_message_attach - Create a message attachment
 * @param m          Mailbox
 * @param e          Email
 * @param attach_msg true if attaching a message
 * @retval ptr  Newly allocated Body
 * @retval NULL Error
 */
struct Body *mutt_make_message_attach(struct Mailbox *m, struct Email *e, bool attach_msg)
{
  char buf[1024];
  struct Body *body = NULL;
  FILE *fp = NULL;
  CopyMessageFlags cmflags;
  SecurityFlags pgp = WithCrypto ? e->security : SEC_NO_FLAGS;

  if (WithCrypto)
  {
    if ((C_MimeForwardDecode || C_ForwardDecrypt) && (e->security & SEC_ENCRYPT))
    {
      if (!crypt_valid_passphrase(e->security))
        return NULL;
    }
  }

  mutt_mktemp(buf, sizeof(buf));
  fp = mutt_file_fopen(buf, "w+");
  if (!fp)
    return NULL;

  body = mutt_body_new();
  body->type = TYPE_MESSAGE;
  body->subtype = mutt_str_strdup("rfc822");
  body->filename = mutt_str_strdup(buf);
  body->unlink = true;
  body->use_disp = false;
  body->disposition = DISP_INLINE;
  body->noconv = true;

  mutt_parse_mime_message(m, e);

  CopyHeaderFlags chflags = CH_XMIT;
  cmflags = MUTT_CM_NO_FLAGS;

  /* If we are attaching a message, ignore C_MimeForwardDecode */
  if (!attach_msg && C_MimeForwardDecode)
  {
    chflags |= CH_MIME | CH_TXTPLAIN;
    cmflags = MUTT_CM_DECODE | MUTT_CM_CHARCONV;
    if (WithCrypto & APPLICATION_PGP)
      pgp &= ~PGP_ENCRYPT;
    if (WithCrypto & APPLICATION_SMIME)
      pgp &= ~SMIME_ENCRYPT;
  }
  else if ((WithCrypto != 0) && C_ForwardDecrypt && (e->security & SEC_ENCRYPT))
  {
    if (((WithCrypto & APPLICATION_PGP) != 0) && mutt_is_multipart_encrypted(e->content))
    {
      chflags |= CH_MIME | CH_NONEWLINE;
      cmflags = MUTT_CM_DECODE_PGP;
      pgp &= ~PGP_ENCRYPT;
    }
    else if (((WithCrypto & APPLICATION_PGP) != 0) &&
             ((mutt_is_application_pgp(e->content) & PGP_ENCRYPT) == PGP_ENCRYPT))
    {
      chflags |= CH_MIME | CH_TXTPLAIN;
      cmflags = MUTT_CM_DECODE | MUTT_CM_CHARCONV;
      pgp &= ~PGP_ENCRYPT;
    }
    else if (((WithCrypto & APPLICATION_SMIME) != 0) &&
             ((mutt_is_application_smime(e->content) & SMIME_ENCRYPT) == SMIME_ENCRYPT))
    {
      chflags |= CH_MIME | CH_TXTPLAIN;
      cmflags = MUTT_CM_DECODE | MUTT_CM_CHARCONV;
      pgp &= ~SMIME_ENCRYPT;
    }
  }

  mutt_copy_message(fp, m, e, cmflags, chflags);

  fflush(fp);
  rewind(fp);

  body->email = email_new();
  body->email->offset = 0;
  /* we don't need the user headers here */
  body->email->env = mutt_rfc822_read_header(fp, body->email, false, false);
  if (WithCrypto)
    body->email->security = pgp;
  mutt_update_encoding(body);
  body->parts = body->email->content;

  mutt_file_fclose(&fp);

  return body;
}

/**
 * run_mime_type_query - Run an external command to determine the MIME type
 * @param att Attachment
 *
 * The command in $mime_type_query_command is run.
 */
static void run_mime_type_query(struct Body *att)
{
  FILE *fp = NULL, *fp_err = NULL;
  char *buf = NULL;
  size_t buflen;
  int dummy = 0;
  pid_t pid;
  struct Buffer *cmd = mutt_buffer_pool_get();

  mutt_buffer_file_expand_fmt_quote(cmd, C_MimeTypeQueryCommand, att->filename);

  pid = mutt_create_filter(mutt_b2s(cmd), NULL, &fp, &fp_err);
  if (pid < 0)
  {
    mutt_error(_("Error running \"%s\""), mutt_b2s(cmd));
    mutt_buffer_pool_release(&cmd);
    return;
  }
  mutt_buffer_pool_release(&cmd);

  buf = mutt_file_read_line(buf, &buflen, fp, &dummy, 0);
  if (buf)
  {
    if (strchr(buf, '/'))
      mutt_parse_content_type(buf, att);
    FREE(&buf);
  }

  mutt_file_fclose(&fp);
  mutt_file_fclose(&fp_err);
  mutt_wait_filter(pid);
}

/**
 * mutt_make_file_attach - Create a file attachment
 * @param path File to attach
 * @retval ptr  Newly allocated Body
 * @retval NULL Error
 */
struct Body *mutt_make_file_attach(const char *path)
{
  struct Body *att = mutt_body_new();
  att->filename = mutt_str_strdup(path);

  if (C_MimeTypeQueryCommand && C_MimeTypeQueryFirst)
    run_mime_type_query(att);

  /* Attempt to determine the appropriate content-type based on the filename
   * suffix.  */
  if (!att->subtype)
    mutt_lookup_mime_type(att, path);

  if (!att->subtype && C_MimeTypeQueryCommand && !C_MimeTypeQueryFirst)
  {
    run_mime_type_query(att);
  }

  struct Content *info = mutt_get_content_info(path, att);
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
      att->subtype = mutt_str_strdup("plain");
    }
    else
    {
      att->type = TYPE_APPLICATION;
      att->subtype = mutt_str_strdup("octet-stream");
    }
  }

  FREE(&info);
  mutt_update_encoding(att);
  return att;
}

/**
 * get_toplevel_encoding - Find the most restrictive encoding type
 * @param a Body to examine
 * @retval num Encoding type, e.g. #ENC_7BIT
 */
static int get_toplevel_encoding(struct Body *a)
{
  int e = ENC_7BIT;

  for (; a; a = a->next)
  {
    if (a->encoding == ENC_BINARY)
      return ENC_BINARY;
    else if (a->encoding == ENC_8BIT)
      e = ENC_8BIT;
  }

  return e;
}

/**
 * check_boundary - check for duplicate boundary
 * @param boundary Boundary to look for
 * @param b        Body parts to check
 * @retval true if duplicate found
 */
static bool check_boundary(const char *boundary, struct Body *b)
{
  char *p = NULL;

  if (b->parts && check_boundary(boundary, b->parts))
    return true;

  if (b->next && check_boundary(boundary, b->next))
    return true;

  p = mutt_param_get(&b->parameter, "boundary");
  if (p && (mutt_str_strcmp(p, boundary) == 0))
  {
    return true;
  }
  return false;
}

/**
 * mutt_make_multipart - Create a multipart email
 * @param b Body of email to start
 * @retval ptr Newly allocated Body
 */
struct Body *mutt_make_multipart(struct Body *b)
{
  struct Body *new_body = mutt_body_new();
  new_body->type = TYPE_MULTIPART;
  new_body->subtype = mutt_str_strdup("mixed");
  new_body->encoding = get_toplevel_encoding(b);
  do
  {
    mutt_generate_boundary(&new_body->parameter);
    if (check_boundary(mutt_param_get(&new_body->parameter, "boundary"), b))
      mutt_param_delete(&new_body->parameter, "boundary");
  } while (!mutt_param_get(&new_body->parameter, "boundary"));
  new_body->use_disp = false;
  new_body->disposition = DISP_INLINE;
  new_body->parts = b;

  return new_body;
}

/**
 * mutt_remove_multipart - Extract the multipart body if it exists
 * @param b Body to alter
 * @retval ptr The parts of the Body
 *
 * @note The original Body is freed
 */
struct Body *mutt_remove_multipart(struct Body *b)
{
  struct Body *t = NULL;

  if (b->parts)
  {
    t = b;
    b = b->parts;
    t->parts = NULL;
    mutt_body_free(&t);
  }
  return b;
}

/**
 * mutt_write_addrlist - wrapper around mutt_write_address()
 * @param al      Address list
 * @param fp      File to write to
 * @param linelen Line length to use
 * @param display True if these addresses will be displayed to the user
 *
 * So we can handle very large recipient lists without needing a huge temporary
 * buffer in memory
 */
void mutt_write_addrlist(struct AddressList *al, FILE *fp, int linelen, bool display)
{
  char buf[1024];
  int count = 0;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    buf[0] = '\0';
    mutt_addr_write(buf, sizeof(buf), a, display);
    size_t len = mutt_str_strlen(buf);
    if (count && (linelen + len > 74))
    {
      fputs("\n\t", fp);
      linelen = len + 8; /* tab is usually about 8 spaces... */
    }
    else
    {
      if (count && a->mailbox)
      {
        fputc(' ', fp);
        linelen++;
      }
      linelen += len;
    }
    fputs(buf, fp);
    struct Address *next = TAILQ_NEXT(a, entries);
    if (!a->group && next && next->mailbox)
    {
      linelen++;
      fputc(',', fp);
    }
    count++;
  }
  fputc('\n', fp);
}

/**
 * mutt_write_references - Add the message references to a list
 * @param r    String List of references
 * @param fp   File to write to
 * @param trim Trim the list to at most this many items
 *
 * Write the list in reverse because they are stored in reverse order when
 * parsed to speed up threading.
 */
void mutt_write_references(const struct ListHead *r, FILE *fp, size_t trim)
{
  struct ListNode *np = NULL;
  size_t length = 0;

  STAILQ_FOREACH(np, r, entries)
  {
    if (++length == trim)
      break;
  }

  struct ListNode **ref = mutt_mem_calloc(length, sizeof(struct ListNode *));

  // store in reverse order
  size_t tmp = length;
  STAILQ_FOREACH(np, r, entries)
  {
    ref[--tmp] = np;
    if (tmp == 0)
      break;
  }

  for (size_t i = 0; i < length; i++)
  {
    fputc(' ', fp);
    fputs(ref[i]->data, fp);
    if (i != length - 1)
      fputc('\n', fp);
  }

  FREE(&ref);
}

/**
 * print_val - Add pieces to an email header, wrapping where necessary
 * @param fp      File to write to
 * @param pfx     Prefix for headers
 * @param value   Text to be added
 * @param chflags Flags, see #CopyHeaderFlags
 * @param col     Column that this text starts at
 * @retval  0 Success
 * @retval -1 Failure
 */
static int print_val(FILE *fp, const char *pfx, const char *value,
                     CopyHeaderFlags chflags, size_t col)
{
  while (value && (value[0] != '\0'))
  {
    if (fputc(*value, fp) == EOF)
      return -1;
    /* corner-case: break words longer than 998 chars by force,
     * mandated by RFC5322 */
    if (!(chflags & CH_DISPLAY) && (++col >= 998))
    {
      if (fputs("\n ", fp) < 0)
        return -1;
      col = 1;
    }
    if (*value == '\n')
    {
      if ((value[1] != '\0') && pfx && (pfx[0] != '\0') && (fputs(pfx, fp) == EOF))
        return -1;
      /* for display, turn folding spaces into folding tabs */
      if ((chflags & CH_DISPLAY) && ((value[1] == ' ') || (value[1] == '\t')))
      {
        value++;
        while ((value[0] != '\0') && ((value[0] == ' ') || (value[0] == '\t')))
          value++;
        if (fputc('\t', fp) == EOF)
          return -1;
        continue;
      }
    }
    value++;
  }
  return 0;
}

/**
 * fold_one_header - Fold one header line
 * @param fp      File to write to
 * @param tag     Header key, e.g. "From"
 * @param value   Header value
 * @param pfx     Prefix for header
 * @param wraplen Column to wrap at
 * @param chflags Flags, see #CopyHeaderFlags
 * @retval  0 Success
 * @retval -1 Failure
 */
static int fold_one_header(FILE *fp, const char *tag, const char *value,
                           const char *pfx, int wraplen, CopyHeaderFlags chflags)
{
  const char *p = value;
  char buf[8192] = { 0 };
  int first = 1, col = 0, l = 0;
  const bool display = (chflags & CH_DISPLAY);

  mutt_debug(LL_DEBUG5, "pfx=[%s], tag=[%s], flags=%d value=[%s]\n", pfx, tag,
             chflags, NONULL(value));

  if (tag && *tag && (fprintf(fp, "%s%s: ", NONULL(pfx), tag) < 0))
    return -1;
  col = mutt_str_strlen(tag) + ((tag && (tag[0] != '\0')) ? 2 : 0) + mutt_str_strlen(pfx);

  while (p && (p[0] != '\0'))
  {
    int fold = 0;

    /* find the next word and place it in 'buf'. it may start with
     * whitespace we can fold before */
    const char *next = mutt_str_find_word(p);
    l = MIN(sizeof(buf) - 1, next - p);
    memcpy(buf, p, l);
    buf[l] = '\0';

    /* determine width: character cells for display, bytes for sending
     * (we get pure ascii only) */
    const int w = mutt_mb_width(buf, col, display);
    const int enc = mutt_str_startswith(buf, "=?", CASE_MATCH);

    mutt_debug(LL_DEBUG5, "word=[%s], col=%d, w=%d, next=[0x0%x]\n", buf, col, w, *next);

    /* insert a folding \n before the current word's lwsp except for
     * header name, first word on a line (word longer than wrap width)
     * and encoded words */
    if (!first && !enc && col && ((col + w) >= wraplen))
    {
      col = mutt_str_strlen(pfx);
      fold = 1;
      if (fprintf(fp, "\n%s", NONULL(pfx)) <= 0)
        return -1;
    }

    /* print the actual word; for display, ignore leading ws for word
     * and fold with tab for readability */
    if (display && fold)
    {
      char *pc = buf;
      while ((pc[0] != '\0') && ((pc[0] == ' ') || (pc[0] == '\t')))
      {
        pc++;
        col--;
      }
      if (fputc('\t', fp) == EOF)
        return -1;
      if (print_val(fp, pfx, pc, chflags, col) < 0)
        return -1;
      col += 8;
    }
    else if (print_val(fp, pfx, buf, chflags, col) < 0)
      return -1;
    col += w;

    /* if the current word ends in \n, ignore all its trailing spaces
     * and reset column; this prevents us from putting only spaces (or
     * even none) on a line if the trailing spaces are located at our
     * current line width
     * XXX this covers ASCII space only, for display we probably
     * want something like iswspace() here */
    const char *sp = next;
    while ((sp[0] != '\0') && ((sp[0] == ' ') || (sp[0] == '\t')))
      sp++;
    if (sp[0] == '\n')
    {
      next = sp;
      col = 0;
    }

    p = next;
    first = 0;
  }

  /* if we have printed something but didn't \n-terminate it, do it
   * except the last word we printed ended in \n already */
  if (col && ((l == 0) || (buf[l - 1] != '\n')))
    if (putc('\n', fp) == EOF)
      return -1;

  return 0;
}

/**
 * unfold_header - Unfold a wrapped email header
 * @param s String to process
 * @retval ptr Unfolded string
 *
 * @note The string is altered in-place
 */
static char *unfold_header(char *s)
{
  char *p = s;
  char *q = s;

  while (p && (p[0] != '\0'))
  {
    /* remove CRLF prior to FWSP, turn \t into ' ' */
    if ((p[0] == '\r') && (p[1] == '\n') && ((p[2] == ' ') || (p[2] == '\t')))
    {
      *q++ = ' ';
      p += 3;
      continue;
    }
    /* remove LF prior to FWSP, turn \t into ' ' */
    else if ((p[0] == '\n') && ((p[1] == ' ') || (p[1] == '\t')))
    {
      *q++ = ' ';
      p += 2;
      continue;
    }
    *q++ = *p++;
  }
  if (q)
    q[0] = '\0';

  return s;
}

/**
 * write_one_header - Write out one header line
 * @param fp      File to write to
 * @param pfxw    Width of prefix string
 * @param max     Max width
 * @param wraplen Column to wrap at
 * @param pfx     Prefix for header
 * @param start   Start of header line
 * @param end     End of header line
 * @param chflags Flags, see #CopyHeaderFlags
 * @retval  0 Success
 * @retval -1 Failure
 */
static int write_one_header(FILE *fp, int pfxw, int max, int wraplen, const char *pfx,
                            const char *start, const char *end, CopyHeaderFlags chflags)
{
  char *tagbuf = NULL, *valbuf = NULL, *t = NULL;
  bool is_from = ((end - start) > 5) && mutt_str_startswith(start, "from ", CASE_IGNORE);

  /* only pass through folding machinery if necessary for sending,
   * never wrap From_ headers on sending */
  if (!(chflags & CH_DISPLAY) && ((pfxw + max <= wraplen) || is_from))
  {
    valbuf = mutt_str_substr_dup(start, end);
    mutt_debug(LL_DEBUG5, "buf[%s%s] short enough, max width = %d <= %d\n",
               NONULL(pfx), valbuf, max, wraplen);
    if (pfx && *pfx)
    {
      if (fputs(pfx, fp) == EOF)
      {
        FREE(&valbuf);
        return -1;
      }
    }

    t = strchr(valbuf, ':');
    if (!t)
    {
      mutt_debug(LL_DEBUG1, "#1 warning: header not in 'key: value' format!\n");
      FREE(&valbuf);
      return 0;
    }
    if (print_val(fp, pfx, valbuf, chflags, mutt_str_strlen(pfx)) < 0)
    {
      FREE(&valbuf);
      return -1;
    }
    FREE(&valbuf);
  }
  else
  {
    t = strchr(start, ':');
    if (!t || (t > end))
    {
      mutt_debug(LL_DEBUG1, "#2 warning: header not in 'key: value' format!\n");
      return 0;
    }
    if (is_from)
    {
      tagbuf = NULL;
      valbuf = mutt_str_substr_dup(start, end);
    }
    else
    {
      tagbuf = mutt_str_substr_dup(start, t);
      /* skip over the colon separating the header field name and value */
      t++;

      /* skip over any leading whitespace (WSP, as defined in RFC5322)
       * NOTE: mutt_str_skip_email_wsp() does the wrong thing here.
       *       See tickets 3609 and 3716. */
      while ((*t == ' ') || (*t == '\t'))
        t++;

      valbuf = mutt_str_substr_dup(t, end);
    }
    mutt_debug(LL_DEBUG2, "buf[%s%s] too long, max width = %d > %d\n",
               NONULL(pfx), NONULL(valbuf), max, wraplen);
    if (fold_one_header(fp, tagbuf, valbuf, pfx, wraplen, chflags) < 0)
    {
      FREE(&valbuf);
      FREE(&tagbuf);
      return -1;
    }
    FREE(&tagbuf);
    FREE(&valbuf);
  }
  return 0;
}

/**
 * mutt_write_one_header - Write one header line to a file
 * @param fp      File to write to
 * @param tag     Header key, e.g. "From"
 * @param value   Header value
 * @param pfx     Prefix for header
 * @param wraplen Column to wrap at
 * @param chflags Flags, see #CopyHeaderFlags
 * @retval  0 Success
 * @retval -1 Failure
 *
 * split several headers into individual ones and call write_one_header
 * for each one
 */
int mutt_write_one_header(FILE *fp, const char *tag, const char *value,
                          const char *pfx, int wraplen, CopyHeaderFlags chflags)
{
  char *last = NULL, *line = NULL;
  int max = 0, w, rc = -1;
  int pfxw = mutt_strwidth(pfx);
  char *v = mutt_str_strdup(value);
  bool display = (chflags & CH_DISPLAY);

  if (!display || C_Weed)
    v = unfold_header(v);

  /* when not displaying, use sane wrap value */
  if (!display)
  {
    if ((C_WrapHeaders < 78) || (C_WrapHeaders > 998))
      wraplen = 78;
    else
      wraplen = C_WrapHeaders;
  }
  else if ((wraplen <= 0) || (wraplen > MuttIndexWindow->cols))
    wraplen = MuttIndexWindow->cols;

  if (tag)
  {
    /* if header is short enough, simply print it */
    if (!display && (mutt_strwidth(tag) + 2 + pfxw + mutt_strwidth(v) <= wraplen))
    {
      mutt_debug(LL_DEBUG5, "buf[%s%s: %s] is short enough\n", NONULL(pfx), tag, v);
      if (fprintf(fp, "%s%s: %s\n", NONULL(pfx), tag, v) <= 0)
        goto out;
      rc = 0;
      goto out;
    }
    else
    {
      rc = fold_one_header(fp, tag, v, pfx, wraplen, chflags);
      goto out;
    }
  }

  char *p = v;
  last = v;
  line = v;
  while (p && *p)
  {
    p = strchr(p, '\n');

    /* find maximum line width in current header */
    if (p)
      *p = '\0';
    w = mutt_mb_width(line, 0, display);
    if (w > max)
      max = w;
    if (p)
      *p = '\n';

    if (!p)
      break;

    line = ++p;
    if ((*p != ' ') && (*p != '\t'))
    {
      if (write_one_header(fp, pfxw, max, wraplen, pfx, last, p, chflags) < 0)
        goto out;
      last = p;
      max = 0;
    }
  }

  if (last && *last)
    if (write_one_header(fp, pfxw, max, wraplen, pfx, last, p, chflags) < 0)
      goto out;

  rc = 0;

out:
  FREE(&v);
  return rc;
}

/**
 * The next array/enum pair is used to to keep track of user headers that
 * override pre-defined headers NeoMutt would emit. Keep the array sorted and
 * in sync with the enum.
 */
static const char *const userhdrs_override_headers[] = {
  "content-type:",
  "user-agent:",
};

enum UserHdrsOverrideIdx
{
  USERHDRS_OVERRIDE_CONTENT_TYPE,
  USERHDRS_OVERRIDE_USER_AGENT,
};

struct UserHdrsOverride
{
  bool is_overridden[mutt_array_size(userhdrs_override_headers)];
};

/**
 * userhdrs_override_cmp - Compare a user-defined header with an element of the userhdrs_override_headers list
 * @param a Pointer to the string containing the user-defined header
 * @param b Pointer to an element of the userhdrs_override_headers list
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int userhdrs_override_cmp(const void *a, const void *b)
{
  const char *ca = a;
  const char *cb = *(const char **) b;
  return mutt_str_strncasecmp(ca, cb, strlen(cb));
}

/**
 * write_userhdrs - Write user-defined headers and keep track of the interesting ones
 * @param fp       FILE pointer where to write the headers
 * @param userhdrs List of headers to write
 * @param privacy  Omit headers that could identify the user
 * @retval obj UserHdrsOverride struct containing a bitmask of which unique headers were written
 */
static struct UserHdrsOverride write_userhdrs(FILE *fp, const struct ListHead *userhdrs, bool privacy)
{
  struct UserHdrsOverride overrides = { { 0 } };

  struct ListNode *tmp = NULL;
  STAILQ_FOREACH(tmp, userhdrs, entries)
  {
    char *const colon = strchr(tmp->data, ':');
    if (!colon)
    {
      continue;
    }

    const char *const value = mutt_str_skip_email_wsp(colon + 1);
    if (*value == '\0')
    {
      continue; /* don't emit empty fields. */
    }

    /* check whether the current user-header is an override */
    size_t curr_override = (size_t) -1;
    const char *const *idx = bsearch(tmp->data, userhdrs_override_headers,
                                     mutt_array_size(userhdrs_override_headers),
                                     sizeof(char *), userhdrs_override_cmp);
    if (idx != NULL)
    {
      curr_override = idx - userhdrs_override_headers;
      overrides.is_overridden[curr_override] = true;
    }

    if (privacy && (curr_override == USERHDRS_OVERRIDE_USER_AGENT))
    {
      continue;
    }

    *colon = '\0';
    mutt_write_one_header(fp, tmp->data, value, NULL, 0, CH_NO_FLAGS);
    *colon = ':';
  }

  return overrides;
}

/**
 * mutt_rfc822_write_header - Write out one RFC822 header line
 * @param fp      File to write to
 * @param env     Envelope of email
 * @param attach  Attachment
 * @param mode    Mode, see #MuttWriteHeaderMode
 * @param privacy If true, remove headers that might identify the user
 * @param hide_protected_subject If true, replace subject header
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Note: all RFC2047 encoding should be done outside of this routine, except
 * for the "real name."  This will allow this routine to be used more than
 * once, if necessary.
 *
 * Likewise, all IDN processing should happen outside of this routine.
 *
 * privacy true => will omit any headers which may identify the user.
 *               Output generated is suitable for being sent through
 *               anonymous remailer chains.
 *
 * hide_protected_subject: replaces the Subject header with
 * $crypt_protected_headers_subject in NORMAL or POSTPONE mode.
 */
int mutt_rfc822_write_header(FILE *fp, struct Envelope *env,
                             struct Body *attach, enum MuttWriteHeaderMode mode,
                             bool privacy, bool hide_protected_subject)
{
  char buf[1024];

  if ((mode == MUTT_WRITE_HEADER_NORMAL) && !privacy)
    fputs(mutt_date_make_date(buf, sizeof(buf)), fp);

  /* UseFrom is not consulted here so that we can still write a From:
   * field if the user sets it with the 'my_hdr' command */
  if (!TAILQ_EMPTY(&env->from) && !privacy)
  {
    buf[0] = '\0';
    mutt_addrlist_write(buf, sizeof(buf), &env->from, false);
    fprintf(fp, "From: %s\n", buf);
  }

  if (!TAILQ_EMPTY(&env->sender) && !privacy)
  {
    buf[0] = '\0';
    mutt_addrlist_write(buf, sizeof(buf), &env->sender, false);
    fprintf(fp, "Sender: %s\n", buf);
  }

  if (!TAILQ_EMPTY(&env->to))
  {
    fputs("To: ", fp);
    mutt_write_addrlist(&env->to, fp, 4, 0);
  }
  else if (mode == MUTT_WRITE_HEADER_EDITHDRS)
#ifdef USE_NNTP
    if (!OptNewsSend)
#endif
      fputs("To:\n", fp);

  if (!TAILQ_EMPTY(&env->cc))
  {
    fputs("Cc: ", fp);
    mutt_write_addrlist(&env->cc, fp, 4, 0);
  }
  else if (mode == MUTT_WRITE_HEADER_EDITHDRS)
#ifdef USE_NNTP
    if (!OptNewsSend)
#endif
      fputs("Cc:\n", fp);

  if (!TAILQ_EMPTY(&env->bcc))
  {
    if ((mode == MUTT_WRITE_HEADER_POSTPONE) || (mode == MUTT_WRITE_HEADER_EDITHDRS) ||
        ((mode == MUTT_WRITE_HEADER_NORMAL) && C_WriteBcc))
    {
      fputs("Bcc: ", fp);
      mutt_write_addrlist(&env->bcc, fp, 5, 0);
    }
  }
  else if (mode == MUTT_WRITE_HEADER_EDITHDRS)
#ifdef USE_NNTP
    if (!OptNewsSend)
#endif
      fputs("Bcc:\n", fp);

#ifdef USE_NNTP
  if (env->newsgroups)
    fprintf(fp, "Newsgroups: %s\n", env->newsgroups);
  else if ((mode == MUTT_WRITE_HEADER_EDITHDRS) && OptNewsSend)
    fputs("Newsgroups:\n", fp);

  if (env->followup_to)
    fprintf(fp, "Followup-To: %s\n", env->followup_to);
  else if ((mode == MUTT_WRITE_HEADER_EDITHDRS) && OptNewsSend)
    fputs("Followup-To:\n", fp);

  if (env->x_comment_to)
    fprintf(fp, "X-Comment-To: %s\n", env->x_comment_to);
  else if ((mode == MUTT_WRITE_HEADER_EDITHDRS) && OptNewsSend && C_XCommentTo)
    fputs("X-Comment-To:\n", fp);
#endif

  if (env->subject)
  {
    if (hide_protected_subject &&
        ((mode == MUTT_WRITE_HEADER_NORMAL) || (mode == MUTT_WRITE_HEADER_POSTPONE)))
      mutt_write_one_header(fp, "Subject", C_CryptProtectedHeadersSubject, NULL, 0, CH_NO_FLAGS);
    else
      mutt_write_one_header(fp, "Subject", env->subject, NULL, 0, CH_NO_FLAGS);
  }
  else if (mode == MUTT_WRITE_HEADER_EDITHDRS)
    fputs("Subject:\n", fp);

  /* save message id if the user has set it */
  if (env->message_id && !privacy)
    fprintf(fp, "Message-ID: %s\n", env->message_id);

  if (!TAILQ_EMPTY(&env->reply_to))
  {
    fputs("Reply-To: ", fp);
    mutt_write_addrlist(&env->reply_to, fp, 10, 0);
  }
  else if (mode == MUTT_WRITE_HEADER_EDITHDRS)
    fputs("Reply-To:\n", fp);

  if (!TAILQ_EMPTY(&env->mail_followup_to))
  {
#ifdef USE_NNTP
    if (!OptNewsSend)
#endif
    {
      fputs("Mail-Followup-To: ", fp);
      mutt_write_addrlist(&env->mail_followup_to, fp, 18, 0);
    }
  }

  /* Add any user defined headers */
  struct UserHdrsOverride userhdrs_overrides = write_userhdrs(fp, &env->userhdrs, privacy);

  if ((mode == MUTT_WRITE_HEADER_NORMAL) || (mode == MUTT_WRITE_HEADER_POSTPONE))
  {
    if (!STAILQ_EMPTY(&env->references))
    {
      fputs("References:", fp);
      mutt_write_references(&env->references, fp, 10);
      fputc('\n', fp);
    }

    /* Add the MIME headers */
    if (!userhdrs_overrides.is_overridden[USERHDRS_OVERRIDE_CONTENT_TYPE])
    {
      fputs("MIME-Version: 1.0\n", fp);
      mutt_write_mime_header(attach, fp);
    }
  }

  if (!STAILQ_EMPTY(&env->in_reply_to))
  {
    fputs("In-Reply-To:", fp);
    mutt_write_references(&env->in_reply_to, fp, 0);
    fputc('\n', fp);
  }

#ifdef USE_AUTOCRYPT
  if (C_Autocrypt)
  {
    if (mode == MUTT_WRITE_HEADER_NORMAL)
      mutt_autocrypt_write_autocrypt_header(env, fp);
    if (mode == MUTT_WRITE_HEADER_MIME)
      mutt_autocrypt_write_gossip_headers(env, fp);
  }
#endif

  if ((mode == MUTT_WRITE_HEADER_NORMAL) && !privacy && C_UserAgent &&
      !userhdrs_overrides.is_overridden[USERHDRS_OVERRIDE_USER_AGENT])
  {
    /* Add a vanity header */
    fprintf(fp, "User-Agent: NeoMutt/%s%s\n", PACKAGE_VERSION, GitVer);
  }

  return (ferror(fp) == 0) ? 0 : -1;
}

/**
 * encode_headers - RFC2047-encode a list of headers
 * @param h String List of headers
 *
 * The strings are encoded in-place.
 */
static void encode_headers(struct ListHead *h)
{
  char *tmp = NULL;
  char *p = NULL;
  int i;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, h, entries)
  {
    p = strchr(np->data, ':');
    if (!p)
      continue;

    i = p - np->data;
    p = mutt_str_skip_email_wsp(p + 1);
    tmp = mutt_str_strdup(p);

    if (!tmp)
      continue;

    rfc2047_encode(&tmp, NULL, i + 2, C_SendCharset);
    mutt_mem_realloc(&np->data, i + 2 + mutt_str_strlen(tmp) + 1);

    sprintf(np->data + i + 2, "%s", tmp);

    FREE(&tmp);
  }
}

/**
 * mutt_fqdn - Get the Fully-Qualified Domain Name
 * @param may_hide_host If true, hide the hostname (leaving just the domain)
 * @retval ptr  string pointer into Hostname
 * @retval NULL Error, e.g no Hostname
 *
 * @warning Do not free the returned pointer
 */
const char *mutt_fqdn(bool may_hide_host)
{
  if (!C_Hostname || (C_Hostname[0] == '@'))
    return NULL;

  char *p = C_Hostname;

  if (may_hide_host && C_HiddenHost)
  {
    p = strchr(C_Hostname, '.');
    if (p)
      p++;

    // sanity check: don't hide the host if the fqdn is something like example.com
    if (!p || !strchr(p, '.'))
      p = C_Hostname;
  }

  return p;
}

/**
 * gen_msgid - Generate a unique Message ID
 * @retval ptr Message ID
 *
 * @note The caller should free the string
 */
static char *gen_msgid(void)
{
  char buf[128];
  unsigned char rndid[MUTT_RANDTAG_LEN + 1];

  mutt_rand_base32(rndid, sizeof(rndid) - 1);
  rndid[MUTT_RANDTAG_LEN] = 0;
  const char *fqdn = mutt_fqdn(false);
  if (!fqdn)
    fqdn = NONULL(ShortHostname);

  struct tm tm = mutt_date_gmtime(MUTT_DATE_NOW);
  snprintf(buf, sizeof(buf), "<%d%02d%02d%02d%02d%02d.%s@%s>", tm.tm_year + 1900,
           tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, rndid, fqdn);
  return mutt_str_strdup(buf);
}

/**
 * alarm_handler - Async notification of an alarm signal
 * @param sig Signal, (SIGALRM)
 */
static void alarm_handler(int sig)
{
  SigAlrm = 1;
}

/**
 * send_msg - invoke sendmail in a subshell
 * @param[in]  path     Path to program to execute
 * @param[in]  args     Arguments to pass to program
 * @param[in]  msg      Temp file containing message to send
 * @param[out] tempfile If sendmail is put in the background, this points
 *                      to the temporary file containing the stdout of the
 *                      child process. If it is NULL, stderr and stdout
 *                      are not redirected.
 * @retval  0 Success
 * @retval >0 Failure, return code from sendmail
 */
static int send_msg(const char *path, char **args, const char *msg, char **tempfile)
{
  sigset_t set;
  int st;

  mutt_sig_block_system();

  sigemptyset(&set);
  /* we also don't want to be stopped right now */
  sigaddset(&set, SIGTSTP);
  sigprocmask(SIG_BLOCK, &set, NULL);

  if ((C_SendmailWait >= 0) && tempfile)
  {
    char tmp[PATH_MAX];

    mutt_mktemp(tmp, sizeof(tmp));
    *tempfile = mutt_str_strdup(tmp);
  }

  pid_t pid = fork();
  if (pid == 0)
  {
    struct sigaction act, oldalrm;

    /* save parent's ID before setsid() */
    pid_t ppid = getppid();

    /* we want the delivery to continue even after the main process dies,
     * so we put ourselves into another session right away */
    setsid();

    /* next we close all open files */
    close(0);
#ifdef OPEN_MAX
    for (int fd = tempfile ? 1 : 3; fd < OPEN_MAX; fd++)
      close(fd);
#elif defined(_POSIX_OPEN_MAX)
    for (int fd = tempfile ? 1 : 3; fd < _POSIX_OPEN_MAX; fd++)
      close(fd);
#else
    if (tempfile)
    {
      close(1);
      close(2);
    }
#endif

    /* now the second fork() */
    pid = fork();
    if (pid == 0)
    {
      /* "msg" will be opened as stdin */
      if (open(msg, O_RDONLY, 0) < 0)
      {
        unlink(msg);
        _exit(S_ERR);
      }
      unlink(msg);

      if ((C_SendmailWait >= 0) && tempfile && *tempfile)
      {
        /* *tempfile will be opened as stdout */
        if (open(*tempfile, O_WRONLY | O_APPEND | O_CREAT | O_EXCL, 0600) < 0)
          _exit(S_ERR);
        /* redirect stderr to *tempfile too */
        if (dup(1) < 0)
          _exit(S_ERR);
      }
      else if (tempfile)
      {
        if (open("/dev/null", O_WRONLY | O_APPEND) < 0) /* stdout */
          _exit(S_ERR);
        if (open("/dev/null", O_RDWR | O_APPEND) < 0) /* stderr */
          _exit(S_ERR);
      }

      /* execvpe is a glibc extension */
      /* execvpe (path, args, mutt_envlist_getlist()); */
      execvp(path, args);
      _exit(S_ERR);
    }
    else if (pid == -1)
    {
      unlink(msg);
      FREE(tempfile);
      _exit(S_ERR);
    }

    /* C_SendmailWait > 0: interrupt waitpid() after C_SendmailWait seconds
     * C_SendmailWait = 0: wait forever
     * C_SendmailWait < 0: don't wait */
    if (C_SendmailWait > 0)
    {
      SigAlrm = 0;
      act.sa_handler = alarm_handler;
#ifdef SA_INTERRUPT
      /* need to make sure waitpid() is interrupted on SIGALRM */
      act.sa_flags = SA_INTERRUPT;
#else
      act.sa_flags = 0;
#endif
      sigemptyset(&act.sa_mask);
      sigaction(SIGALRM, &act, &oldalrm);
      alarm(C_SendmailWait);
    }
    else if (C_SendmailWait < 0)
      _exit(0xff & EX_OK);

    if (waitpid(pid, &st, 0) > 0)
    {
      st = WIFEXITED(st) ? WEXITSTATUS(st) : S_ERR;
      if (C_SendmailWait && (st == (0xff & EX_OK)) && tempfile && *tempfile)
      {
        unlink(*tempfile); /* no longer needed */
        FREE(tempfile);
      }
    }
    else
    {
      st = ((C_SendmailWait > 0) && (errno == EINTR) && SigAlrm) ? S_BKG : S_ERR;
      if ((C_SendmailWait > 0) && tempfile && *tempfile)
      {
        unlink(*tempfile);
        FREE(tempfile);
      }
    }

    if (C_SendmailWait > 0)
    {
      /* reset alarm; not really needed, but... */
      alarm(0);
      sigaction(SIGALRM, &oldalrm, NULL);
    }

    if ((kill(ppid, 0) == -1) && (errno == ESRCH) && tempfile && *tempfile)
    {
      /* the parent is already dead */
      unlink(*tempfile);
      FREE(tempfile);
    }

    _exit(st);
  }

  sigprocmask(SIG_UNBLOCK, &set, NULL);

  if ((pid != -1) && (waitpid(pid, &st, 0) > 0))
    st = WIFEXITED(st) ? WEXITSTATUS(st) : S_ERR; /* return child status */
  else
    st = S_ERR; /* error */

  mutt_sig_unblock_system(true);

  return st;
}

/**
 * add_args_one - Add an Address to a dynamic array
 * @param[out] args    Array to add to
 * @param[out] argslen Number of entries in array
 * @param[out] argsmax Allocated size of the array
 * @param[in]  addr    Address to add
 * @retval ptr Updated array
 */
static char **add_args_one(char **args, size_t *argslen, size_t *argsmax, struct Address *addr)
{
  /* weed out group mailboxes, since those are for display only */
  if (addr->mailbox && !addr->group)
  {
    if (*argslen == *argsmax)
      mutt_mem_realloc(&args, (*argsmax += 5) * sizeof(char *));
    args[(*argslen)++] = addr->mailbox;
  }
  return args;
}

/**
 * add_args - Add a list of Addresses to a dynamic array
 * @param[out] args    Array to add to
 * @param[out] argslen Number of entries in array
 * @param[out] argsmax Allocated size of the array
 * @param[in]  al      Addresses to add
 * @retval ptr Updated array
 */
static char **add_args(char **args, size_t *argslen, size_t *argsmax, struct AddressList *al)
{
  if (!al)
    return args;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    args = add_args_one(args, argslen, argsmax, a);
  }
  return args;
}

/**
 * add_option - Add a string to a dynamic array
 * @param[out] args    Array to add to
 * @param[out] argslen Number of entries in array
 * @param[out] argsmax Allocated size of the array
 * @param[in]  s       string to add
 * @retval ptr Updated array
 *
 * @note The array may be realloc()'d
 */
static char **add_option(char **args, size_t *argslen, size_t *argsmax, char *s)
{
  if (*argslen == *argsmax)
    mutt_mem_realloc(&args, (*argsmax += 5) * sizeof(char *));
  args[(*argslen)++] = s;
  return args;
}

/**
 * mutt_invoke_sendmail - Run sendmail
 * @param from     The sender
 * @param to       Recipients
 * @param cc       Recipients
 * @param bcc      Recipients
 * @param msg      File containing message
 * @param eightbit Message contains 8bit chars
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_invoke_sendmail(struct AddressList *from, struct AddressList *to,
                         struct AddressList *cc, struct AddressList *bcc,
                         const char *msg, int eightbit)
{
  char *ps = NULL, *path = NULL, *s = NULL, *childout = NULL;
  char **args = NULL;
  size_t argslen = 0, argsmax = 0;
  char **extra_args = NULL;
  int i;

#ifdef USE_NNTP
  if (OptNewsSend)
  {
    char cmd[1024];

    mutt_expando_format(cmd, sizeof(cmd), 0, sizeof(cmd), NONULL(C_Inews),
                        nntp_format_str, 0, MUTT_FORMAT_NO_FLAGS);
    if (!*cmd)
    {
      i = nntp_post(Context->mailbox, msg);
      unlink(msg);
      return i;
    }

    s = mutt_str_strdup(cmd);
  }
  else
#endif
    s = mutt_str_strdup(C_Sendmail);

  /* ensure that $sendmail is set to avoid a crash. http://dev.mutt.org/trac/ticket/3548 */
  if (!s)
  {
    mutt_error(_("$sendmail must be set in order to send mail"));
    return -1;
  }

  ps = s;
  i = 0;
  while ((ps = strtok(ps, " ")))
  {
    if (argslen == argsmax)
      mutt_mem_realloc(&args, sizeof(char *) * (argsmax += 5));

    if (i)
    {
      if (mutt_str_strcmp(ps, "--") == 0)
        break;
      args[argslen++] = ps;
    }
    else
    {
      path = mutt_str_strdup(ps);
      ps = strrchr(ps, '/');
      if (ps)
        ps++;
      else
        ps = path;
      args[argslen++] = ps;
    }
    ps = NULL;
    i++;
  }

#ifdef USE_NNTP
  if (!OptNewsSend)
  {
#endif
    size_t extra_argslen = 0;
    /* If C_Sendmail contained a "--", we save the recipients to append to
   * args after other possible options added below. */
    if (ps)
    {
      ps = NULL;
      size_t extra_argsmax = 0;
      while ((ps = strtok(ps, " ")))
      {
        if (extra_argslen == extra_argsmax)
          mutt_mem_realloc(&extra_args, sizeof(char *) * (extra_argsmax += 5));

        extra_args[extra_argslen++] = ps;
        ps = NULL;
      }
    }

    if (eightbit && C_Use8bitmime)
      args = add_option(args, &argslen, &argsmax, "-B8BITMIME");

    if (C_UseEnvelopeFrom)
    {
      if (C_EnvelopeFromAddress)
      {
        args = add_option(args, &argslen, &argsmax, "-f");
        args = add_args_one(args, &argslen, &argsmax, C_EnvelopeFromAddress);
      }
      else if (!TAILQ_EMPTY(from) && !TAILQ_NEXT(TAILQ_FIRST(from), entries))
      {
        args = add_option(args, &argslen, &argsmax, "-f");
        args = add_args(args, &argslen, &argsmax, from);
      }
    }

    if (C_DsnNotify)
    {
      args = add_option(args, &argslen, &argsmax, "-N");
      args = add_option(args, &argslen, &argsmax, C_DsnNotify);
    }
    if (C_DsnReturn)
    {
      args = add_option(args, &argslen, &argsmax, "-R");
      args = add_option(args, &argslen, &argsmax, C_DsnReturn);
    }
    args = add_option(args, &argslen, &argsmax, "--");
    for (i = 0; i < extra_argslen; i++)
      args = add_option(args, &argslen, &argsmax, extra_args[i]);
    args = add_args(args, &argslen, &argsmax, to);
    args = add_args(args, &argslen, &argsmax, cc);
    args = add_args(args, &argslen, &argsmax, bcc);
#ifdef USE_NNTP
  }
#endif

  if (argslen == argsmax)
    mutt_mem_realloc(&args, sizeof(char *) * (++argsmax));

  args[argslen++] = NULL;

  /* Some user's $sendmail command uses gpg for password decryption,
   * and is set up to prompt using ncurses pinentry.  If we
   * mutt_endwin() it leaves other users staring at a blank screen.
   * So instead, just force a hard redraw on the next refresh. */
  if (!OptNoCurses)
    mutt_need_hard_redraw();

  i = send_msg(path, args, msg, OptNoCurses ? NULL : &childout);
  if (i != (EX_OK & 0xff))
  {
    if (i != S_BKG)
    {
      const char *e = mutt_str_sysexit(i);
      mutt_error(_("Error sending message, child exited %d (%s)"), i, NONULL(e));
      if (childout)
      {
        struct stat st;

        if ((stat(childout, &st) == 0) && (st.st_size > 0))
          mutt_do_pager(_("Output of the delivery process"), childout,
                        MUTT_PAGER_NO_FLAGS, NULL);
      }
    }
  }
  else if (childout)
    unlink(childout);

  FREE(&childout);
  FREE(&path);
  FREE(&s);
  FREE(&args);
  FREE(&extra_args);

  if (i == (EX_OK & 0xff))
    i = 0;
  else if (i == S_BKG)
    i = 1;
  else
    i = -1;
  return i;
}

/**
 * mutt_prepare_envelope - Prepare an email header
 * @param env   Envelope to prepare
 * @param final true if this email is going to be sent (not postponed)
 *
 * Encode all the headers prior to sending the email.
 *
 * For postponing (!final) do the necessary encodings only
 */
void mutt_prepare_envelope(struct Envelope *env, bool final)
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

      char buf[1024];
      buf[0] = '\0';
      mutt_addr_cat(buf, sizeof(buf), "undisclosed-recipients", AddressSpecials);

      to->mailbox = mutt_str_strdup(buf);
    }

    mutt_set_followup_to(env);

    if (!env->message_id)
      env->message_id = gen_msgid();
  }

  /* Take care of 8-bit => 7-bit conversion. */
  rfc2047_encode_envelope(env);
  encode_headers(&env->userhdrs);
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
 * @param e           Email
 * @param to          Address to bounce to
 * @param resent_from Address of new sender
 * @param env_from    Envelope of original sender
 * @retval  0 Success
 * @retval -1 Failure
 */
static int bounce_message(FILE *fp, struct Email *e, struct AddressList *to,
                          const char *resent_from, struct AddressList *env_from)
{
  if (!e)
    return -1;

  int rc = 0;
  char tempfile[PATH_MAX];

  mutt_mktemp(tempfile, sizeof(tempfile));
  FILE *fp_tmp = mutt_file_fopen(tempfile, "w");
  if (fp_tmp)
  {
    char date[128];
    CopyHeaderFlags chflags = CH_XMIT | CH_NONEWLINE | CH_NOQFROM;

    if (!C_BounceDelivered)
      chflags |= CH_WEED_DELIVERED;

    fseeko(fp, e->offset, SEEK_SET);
    fprintf(fp_tmp, "Resent-From: %s", resent_from);
    fprintf(fp_tmp, "\nResent-%s", mutt_date_make_date(date, sizeof(date)));
    char *msgid_str = gen_msgid();
    fprintf(fp_tmp, "Resent-Message-ID: %s\n", msgid_str);
    FREE(&msgid_str);
    fputs("Resent-To: ", fp_tmp);
    mutt_write_addrlist(to, fp_tmp, 11, 0);
    mutt_copy_header(fp, e, fp_tmp, chflags, NULL);
    fputc('\n', fp_tmp);
    mutt_file_copy_bytes(fp, fp_tmp, e->content->length);
    if (mutt_file_fclose(&fp_tmp) != 0)
    {
      mutt_perror(tempfile);
      unlink(tempfile);
      return -1;
    }
#ifdef USE_SMTP
    if (C_SmtpUrl)
      rc = mutt_smtp_send(env_from, to, NULL, NULL, tempfile, e->content->encoding == ENC_8BIT);
    else
#endif
      rc = mutt_invoke_sendmail(env_from, to, NULL, NULL, tempfile,
                                e->content->encoding == ENC_8BIT);
  }

  return rc;
}

/**
 * mutt_bounce_message - Bounce an email message
 * @param fp Handle of message
 * @param e  Email
 * @param to AddressList to bounce to
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_bounce_message(FILE *fp, struct Email *e, struct AddressList *to)
{
  if (!fp || !e || !to || TAILQ_EMPTY(to))
    return -1;

  const char *fqdn = mutt_fqdn(true);
  char resent_from[256];
  char *err = NULL;

  resent_from[0] = '\0';
  struct Address *from = mutt_default_from();
  struct AddressList from_list = TAILQ_HEAD_INITIALIZER(from_list);
  mutt_addrlist_append(&from_list, from);

  /* mutt_default_from() does not use $realname if the real name is not set
   * in $from, so we add it here.  The reason it is not added in
   * mutt_default_from() is that during normal sending, we execute
   * send-hooks and set the realname last so that it can be changed based
   * upon message criteria.  */
  if (!from->personal)
    from->personal = mutt_str_strdup(C_Realname);

  mutt_addrlist_qualify(&from_list, fqdn);

  rfc2047_encode_addrlist(&from_list, "Resent-From");
  if (mutt_addrlist_to_intl(&from_list, &err))
  {
    mutt_error(_("Bad IDN %s while preparing resent-from"), err);
    FREE(&err);
    mutt_addrlist_clear(&from_list);
    return -1;
  }
  mutt_addrlist_write(resent_from, sizeof(resent_from), &from_list, false);

#ifdef USE_NNTP
  OptNewsSend = false;
#endif

  /* prepare recipient list. idna conversion appears to happen before this
   * function is called, since the user receives confirmation of the address
   * list being bounced to.  */
  struct AddressList resent_to = TAILQ_HEAD_INITIALIZER(resent_to);
  mutt_addrlist_copy(&resent_to, to, false);
  rfc2047_encode_addrlist(&resent_to, "Resent-To");
  int rc = bounce_message(fp, e, &resent_to, resent_from, &from_list);
  mutt_addrlist_clear(&resent_to);
  mutt_addrlist_clear(&from_list);

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
 * @param[in]  e       Email
 * @param[in]  msgid     Message id
 * @param[in]  post      If true, postpone message
 * @param[in]  fcc       fcc setting to save (postpone only)
 * @param[out] finalpath Final path of email
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_write_multiple_fcc(const char *path, struct Email *e, const char *msgid,
                            bool post, char *fcc, char **finalpath)
{
  char fcc_tok[PATH_MAX];
  char fcc_expanded[PATH_MAX];

  mutt_str_strfcpy(fcc_tok, path, sizeof(fcc_tok));

  char *tok = strtok(fcc_tok, ",");
  if (!tok)
    return -1;

  mutt_debug(LL_DEBUG1, "Fcc: initial mailbox = '%s'\n", tok);
  /* mutt_expand_path already called above for the first token */
  int status = mutt_write_fcc(tok, e, msgid, post, fcc, finalpath);
  if (status != 0)
    return status;

  while ((tok = strtok(NULL, ",")))
  {
    if (!*tok)
      continue;

    /* Only call mutt_expand_path if tok has some data */
    mutt_debug(LL_DEBUG1, "Fcc: additional mailbox token = '%s'\n", tok);
    mutt_str_strfcpy(fcc_expanded, tok, sizeof(fcc_expanded));
    mutt_expand_path(fcc_expanded, sizeof(fcc_expanded));
    mutt_debug(LL_DEBUG1, "     Additional mailbox expanded = '%s'\n", fcc_expanded);
    status = mutt_write_fcc(fcc_expanded, e, msgid, post, fcc, finalpath);
    if (status != 0)
      return status;
  }

  return 0;
}

/**
 * mutt_write_fcc - Write email to FCC mailbox
 * @param[in]  path      Path to mailbox
 * @param[in]  e       Email
 * @param[in]  msgid     Message id
 * @param[in]  post      If true, postpone message
 * @param[in]  fcc       fcc setting to save (postpone only)
 * @param[out] finalpath Final path of email
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_write_fcc(const char *path, struct Email *e, const char *msgid,
                   bool post, char *fcc, char **finalpath)
{
  struct Message *msg = NULL;
  char tempfile[PATH_MAX];
  FILE *fp_tmp = NULL;
  int rc = -1;
  bool need_mailbox_cleanup = false;
  struct stat st;
  char buf[128];
  MsgOpenFlags onm_flags;

  if (post)
    set_noconv_flags(e->content, true);

#ifdef RECORD_FOLDER_HOOK
  mutt_folder_hook(path, NULL);
#endif
  struct Mailbox *m_fcc = mx_path_resolve(path);
  bool old_append = m_fcc->append;
  struct Context *ctx_fcc = mx_mbox_open(m_fcc, MUTT_APPEND | MUTT_QUIET);
  if (!ctx_fcc)
  {
    mutt_debug(LL_DEBUG1, "unable to open mailbox %s in append-mode, aborting\n", path);
    mailbox_free(&m_fcc);
    goto done;
  }

  /* We need to add a Content-Length field to avoid problems where a line in
   * the message body begins with "From " */
  if ((ctx_fcc->mailbox->magic == MUTT_MMDF) || (ctx_fcc->mailbox->magic == MUTT_MBOX))
  {
    mutt_mktemp(tempfile, sizeof(tempfile));
    fp_tmp = mutt_file_fopen(tempfile, "w+");
    if (!fp_tmp)
    {
      mutt_perror(tempfile);
      mx_mbox_close(&ctx_fcc);
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
  msg = mx_msg_open_new(ctx_fcc->mailbox, e, onm_flags);
  if (!msg)
  {
    mutt_file_fclose(&fp_tmp);
    mx_mbox_close(&ctx_fcc);
    goto done;
  }

  /* post == 1 => postpone message.
   * post == 0 => Normal mode.  */
  mutt_rfc822_write_header(
      msg->fp, e->env, e->content, post ? MUTT_WRITE_HEADER_POSTPONE : MUTT_WRITE_HEADER_NORMAL,
      false, C_CryptProtectedHeadersRead && mutt_should_hide_protected_subject(e));

  /* (postponement) if this was a reply of some sort, <msgid> contains the
   * Message-ID: of message replied to.  Save it using a special X-Mutt-
   * header so it can be picked up if the message is recalled at a later
   * point in time.  This will allow the message to be marked as replied if
   * the same mailbox is still open.  */
  if (post && msgid)
    fprintf(msg->fp, "X-Mutt-References: %s\n", msgid);

  /* (postponement) save the Fcc: using a special X-Mutt- header so that
   * it can be picked up when the message is recalled */
  if (post && fcc)
    fprintf(msg->fp, "X-Mutt-Fcc: %s\n", fcc);

  if ((ctx_fcc->mailbox->magic == MUTT_MMDF) || (ctx_fcc->mailbox->magic == MUTT_MBOX))
    fprintf(msg->fp, "Status: RO\n");

  /* mutt_rfc822_write_header() only writes out a Date: header with
   * mode == 0, i.e. _not_ postponement; so write out one ourself */
  if (post)
    fprintf(msg->fp, "%s", mutt_date_make_date(buf, sizeof(buf)));

  /* (postponement) if the mail is to be signed or encrypted, save this info */
  if (((WithCrypto & APPLICATION_PGP) != 0) && post && (e->security & APPLICATION_PGP))
  {
    fputs("X-Mutt-PGP: ", msg->fp);
    if (e->security & SEC_ENCRYPT)
      fputc('E', msg->fp);
    if (e->security & SEC_OPPENCRYPT)
      fputc('O', msg->fp);
    if (e->security & SEC_SIGN)
    {
      fputc('S', msg->fp);
      if (C_PgpSignAs)
        fprintf(msg->fp, "<%s>", C_PgpSignAs);
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
    fputs("X-Mutt-SMIME: ", msg->fp);
    if (e->security & SEC_ENCRYPT)
    {
      fputc('E', msg->fp);
      if (C_SmimeEncryptWith)
        fprintf(msg->fp, "C<%s>", C_SmimeEncryptWith);
    }
    if (e->security & SEC_OPPENCRYPT)
      fputc('O', msg->fp);
    if (e->security & SEC_SIGN)
    {
      fputc('S', msg->fp);
      if (C_SmimeSignAs)
        fprintf(msg->fp, "<%s>", C_SmimeSignAs);
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
    fputs("X-Mutt-Mix:", msg->fp);
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
    mutt_write_mime_body(e->content, fp_tmp);

    /* make sure the last line ends with a newline.  Emacs doesn't ensure this
     * will happen, and it can cause problems parsing the mailbox later.  */
    fseek(fp_tmp, -1, SEEK_END);
    if (fgetc(fp_tmp) != '\n')
    {
      fseek(fp_tmp, 0, SEEK_END);
      fputc('\n', fp_tmp);
    }

    fflush(fp_tmp);
    if (ferror(fp_tmp))
    {
      mutt_debug(LL_DEBUG1, "%s: write failed\n", tempfile);
      mutt_file_fclose(&fp_tmp);
      unlink(tempfile);
      mx_msg_commit(ctx_fcc->mailbox, msg); /* XXX really? */
      mx_msg_close(ctx_fcc->mailbox, &msg);
      mx_mbox_close(&ctx_fcc);
      goto done;
    }

    /* count the number of lines */
    int lines = 0;
    char line_buf[1024];
    rewind(fp_tmp);
    while (fgets(line_buf, sizeof(line_buf), fp_tmp))
      lines++;
    fprintf(msg->fp, "Content-Length: " OFF_T_FMT "\n", (LOFF_T) ftello(fp_tmp));
    fprintf(msg->fp, "Lines: %d\n\n", lines);

    /* copy the body and clean up */
    rewind(fp_tmp);
    rc = mutt_file_copy_stream(fp_tmp, msg->fp);
    if (fclose(fp_tmp) != 0)
      rc = -1;
    /* if there was an error, leave the temp version */
    if (rc == 0)
      unlink(tempfile);
  }
  else
  {
    fputc('\n', msg->fp); /* finish off the header */
    rc = mutt_write_mime_body(e->content, msg->fp);
  }

  if (mx_msg_commit(ctx_fcc->mailbox, msg) != 0)
    rc = -1;
  else if (finalpath)
    *finalpath = mutt_str_strdup(msg->committed_path);
  mx_msg_close(ctx_fcc->mailbox, &msg);
  mx_mbox_close(&ctx_fcc);

  if (!post && need_mailbox_cleanup)
    mutt_mailbox_cleanup(path, &st);

  if (post)
    set_noconv_flags(e->content, false);

done:
  m_fcc->append = old_append;
#ifdef RECORD_FOLDER_HOOK
  /* We ran a folder hook for the destination mailbox,
   * now we run it for the user's current mailbox */
  if (Context && Context->mailbox->path)
    mutt_folder_hook(Context->mailbox->path, Context->mailbox->desc);
#endif

  return rc;
}
