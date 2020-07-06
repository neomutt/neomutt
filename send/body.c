/**
 * @file
 * Write a MIME Email Body to a file
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page send_body Write a MIME Email to a file
 *
 * Write a MIME Email Body to a file
 */

#include "config.h"
#include <stdbool.h>
#include <string.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "body.h"
#include "lib.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "ncrypt/lib.h"

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
    if ((linelen == 4) && mutt_str_startswith(line, "From"))
    {
      mutt_str_copy(line, "=46rom", sizeof(line));
      linelen = 6;
    }
    else if ((linelen == 4) && mutt_str_startswith(line, "from"))
    {
      mutt_str_copy(line, "=66rom", sizeof(line));
      linelen = 6;
    }
    else if ((linelen == 1) && (line[0] == '.'))
    {
      mutt_str_copy(line, "=2E", sizeof(line));
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
 * @param a   Body to use
 * @param fp  File to write to
 * @param sub Config Subset
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_write_mime_body(struct Body *a, FILE *fp, struct ConfigSubset *sub)
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
    mutt_str_copy(boundary, p, sizeof(boundary));

    for (struct Body *t = a->parts; t; t = t->next)
    {
      fprintf(fp, "\n--%s\n", boundary);
      if (mutt_write_mime_header(t, fp, sub) == -1)
        return -1;
      fputc('\n', fp);
      if (mutt_write_mime_body(t, fp, sub) == -1)
        return -1;
    }
    fprintf(fp, "\n--%s--\n", boundary);
    return ferror(fp) ? -1 : 0;
  }

  /* This is pretty gross, but it's the best solution for now... */
  if (((WithCrypto & APPLICATION_PGP) != 0) && (a->type == TYPE_APPLICATION) &&
      mutt_str_equal(a->subtype, "pgp-encrypted") && !a->filename)
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

  mutt_sig_allow_interrupt(true);
  if (a->encoding == ENC_QUOTED_PRINTABLE)
    encode_quoted(fc, fp, write_as_text_part(a));
  else if (a->encoding == ENC_BASE64)
    encode_base64(fc, fp, write_as_text_part(a));
  else if ((a->type == TYPE_TEXT) && (!a->noconv))
    encode_8bit(fc, fp);
  else
    mutt_file_copy_stream(fp_in, fp);
  mutt_sig_allow_interrupt(false);

  mutt_ch_fgetconv_close(&fc);
  mutt_file_fclose(&fp_in);

  if (SigInt == 1)
  {
    SigInt = 0;
    return -1;
  }
  return ferror(fp) ? -1 : 0;
}
