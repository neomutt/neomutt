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
 * @page send_sendlib Miscellaneous functions for sending an email
 *
 * Miscellaneous functions for sending an email
 */

#include "config.h"
#include <errno.h>
#include <iconv.h>
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
#include "context.h"
#include "copy.h"
#include "handler.h"
#include "mutt_globals.h"
#include "mutt_mailbox.h"
#include "mutt_parse.h"
#include "muttlib.h"
#include "mx.h"
#include "options.h"
#include "state.h"
#include "ncrypt/lib.h"

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
      if (ch == '\n')
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

      info->binary = true;
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
    if (!mutt_istr_equal(tocodes[i], "utf-8"))
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
    if (c1)
      tcode[i] = mutt_strn_dup(c, c1 - c);
    else
      tcode[i] = mutt_str_dup(c);
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
      if (c1)
        fcode = mutt_strn_dup(c, c1 - c);
      else
        fcode = mutt_str_dup(c);

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
 * @param sub   Config Subset
 * @retval ptr Newly allocated Content
 *
 * Also set the body charset, sometimes, or not.
 */
struct Content *mutt_get_content_info(const char *fname, struct Body *b,
                                      struct ConfigSubset *sub)
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

  const char *c_charset = cs_subset_string(sub, "charset");

  if (b && (b->type == TYPE_TEXT) && (!b->noconv && !b->force_charset))
  {
    const char *c_attach_charset = cs_subset_string(sub, "attach_charset");
    const char *c_send_charset = cs_subset_string(sub, "send_charset");

    char *chs = mutt_param_get(&b->parameter, "charset");
    const char *fchs =
        b->use_disp ? (c_attach_charset ? c_attach_charset : c_charset) : c_charset;
    if (c_charset && (chs || c_send_charset) &&
        (convert_file_from_to(fp, fchs, chs ? chs : c_send_charset, &fromcode,
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
                        c_charset && !mutt_ch_is_us_ascii(c_charset) ? c_charset : "unknown-8bit"));
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
 * @param a     Body of the email
 * @param fp_in File to read
 * @param sub   Config Subset
 */
static void transform_to_7bit(struct Body *a, FILE *fp_in, struct ConfigSubset *sub)
{
  struct Buffer *buf = NULL;
  struct State s = { 0 };
  struct stat sb;

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
      s.fp_out = mutt_file_fopen(mutt_b2s(buf), "w");
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
      if (stat(a->filename, &sb) == -1)
      {
        mutt_perror("stat");
        return;
      }
      a->length = sb.st_size;

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
      goto cleanup;
    }
    a->length = sb.st_size;
  }

  /* Avoid buffer pool due to recursion */
  mutt_buffer_mktemp(&temp);
  fp_out = mutt_file_fopen(mutt_b2s(&temp), "w+");
  if (!fp_out)
  {
    mutt_perror("fopen");
    goto cleanup;
  }

  fseeko(fp_in, a->offset, SEEK_SET);
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
  if (stat(a->filename, &sb) == -1)
  {
    mutt_perror("stat");
    goto cleanup;
  }
  a->length = sb.st_size;
  mutt_body_free(&a->parts);
  a->email->content = NULL;

cleanup:
  if (fp_in && (fp_in != fp))
    mutt_file_fclose(&fp_in);

  if (fp_out)
  {
    mutt_file_fclose(&fp_out);
    mutt_file_unlink(mutt_b2s(&temp));
  }

  mutt_buffer_dealloc(&temp);
}

/**
 * set_encoding - determine which Content-Transfer-Encoding to use
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
    char send_charset[128];
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
  else if ((b->type == TYPE_APPLICATION) &&
           mutt_istr_equal(b->subtype, "pgp-keys"))
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
 * mutt_update_encoding - Update the encoding type
 * @param a   Body to update
 * @param sub Config Subset
 *
 * Assumes called from send mode where Body->filename points to actual file
 */
void mutt_update_encoding(struct Body *a, struct ConfigSubset *sub)
{
  struct Content *info = NULL;
  char chsbuf[256];

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
  fp = mutt_file_fopen(mutt_b2s(buf), "w+");
  if (!fp)
  {
    mutt_buffer_pool_release(&buf);
    return NULL;
  }

  body = mutt_body_new();
  body->type = TYPE_MESSAGE;
  body->subtype = mutt_str_dup("rfc822");
  body->filename = mutt_str_dup(mutt_b2s(buf));
  body->unlink = true;
  body->use_disp = false;
  body->disposition = DISP_INLINE;
  body->noconv = true;

  mutt_buffer_pool_release(&buf);

  mutt_parse_mime_message(m, e);

  CopyHeaderFlags chflags = CH_XMIT;
  cmflags = MUTT_CM_NO_FLAGS;

  const bool c_forward_decode = cs_subset_bool(sub, "forward_decode");
  /* If we are attaching a message, ignore `$mime_forward_decode` */
  if (!attach_msg && c_forward_decode)
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

  mutt_copy_message(fp, m, e, cmflags, chflags, 0);

  fflush(fp);
  rewind(fp);

  body->email = email_new();
  body->email->offset = 0;
  /* we don't need the user headers here */
  body->email->env = mutt_rfc822_read_header(fp, body->email, false, false);
  if (WithCrypto)
    body->email->security = pgp;
  mutt_update_encoding(body, sub);
  body->parts = body->email->content;

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

  const char *c_mime_type_query_command =
      cs_subset_string(sub, "mime_type_query_command");

  mutt_buffer_file_expand_fmt_quote(cmd, c_mime_type_query_command, att->filename);

  pid = filter_create(mutt_b2s(cmd), NULL, &fp, &fp_err);
  if (pid < 0)
  {
    mutt_error(_("Error running \"%s\""), mutt_b2s(cmd));
    mutt_buffer_pool_release(&cmd);
    return;
  }
  mutt_buffer_pool_release(&cmd);

  buf = mutt_file_read_line(buf, &buflen, fp, NULL, 0);
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
  struct Body *att = mutt_body_new();
  att->filename = mutt_str_dup(path);

  const char *c_mime_type_query_command =
      cs_subset_string(sub, "mime_type_query_command");
  const bool c_mime_type_query_first =
      cs_subset_bool(sub, "mime_type_query_first");

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

  const char *c_send_charset = cs_subset_string(sub, "send_charset");

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
  const char *c_hostname = cs_subset_string(sub, "hostname");
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
 * gen_msgid - Generate a unique Message ID
 * @retval ptr Message ID
 *
 * @note The caller should free the string
 */
static char *gen_msgid(struct ConfigSubset *sub)
{
  char buf[128];
  char rndid[MUTT_RANDTAG_LEN + 1];

  mutt_rand_base32(rndid, sizeof(rndid) - 1);
  rndid[MUTT_RANDTAG_LEN] = 0;
  const char *fqdn = mutt_fqdn(false, sub);
  if (!fqdn)
    fqdn = NONULL(ShortHostname);

  struct tm tm = mutt_date_gmtime(MUTT_DATE_NOW);
  snprintf(buf, sizeof(buf), "<%d%02d%02d%02d%02d%02d.%s@%s>", tm.tm_year + 1900,
           tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, rndid, fqdn);
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

      char buf[1024];
      buf[0] = '\0';
      mutt_addr_cat(buf, sizeof(buf), "undisclosed-recipients", AddressSpecials);

      to->mailbox = mutt_str_dup(buf);
    }

    mutt_set_followup_to(env, sub);

    if (!env->message_id)
      env->message_id = gen_msgid(sub);
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
 * @param e           Email
 * @param to          Address to bounce to
 * @param resent_from Address of new sender
 * @param env_from    Envelope of original sender
 * @param sub         Config Subset
 * @retval  0 Success
 * @retval -1 Failure
 */
static int bounce_message(FILE *fp, struct Email *e, struct AddressList *to,
                          const char *resent_from, struct AddressList *env_from,
                          struct ConfigSubset *sub)
{
  if (!e)
    return -1;

  int rc = 0;

  struct Buffer *tempfile = mutt_buffer_pool_get();
  mutt_buffer_mktemp(tempfile);
  FILE *fp_tmp = mutt_file_fopen(mutt_b2s(tempfile), "w");
  if (fp_tmp)
  {
    CopyHeaderFlags chflags = CH_XMIT | CH_NONEWLINE | CH_NOQFROM;

    const bool c_bounce_delivered = cs_subset_bool(sub, "bounce_delivered");
    if (!c_bounce_delivered)
      chflags |= CH_WEED_DELIVERED;

    fseeko(fp, e->offset, SEEK_SET);
    fprintf(fp_tmp, "Resent-From: %s\n", resent_from);

    struct Buffer *date = mutt_buffer_pool_get();
    mutt_date_make_date(date);
    fprintf(fp_tmp, "Resent-Date: %s\n", mutt_b2s(date));
    mutt_buffer_pool_release(&date);

    char *msgid_str = gen_msgid(sub);
    fprintf(fp_tmp, "Resent-Message-ID: %s\n", msgid_str);
    FREE(&msgid_str);
    fputs("Resent-To: ", fp_tmp);
    mutt_addrlist_write_file(to, fp_tmp, 11, false);
    mutt_copy_header(fp, e, fp_tmp, chflags, NULL, 0);
    fputc('\n', fp_tmp);
    mutt_file_copy_bytes(fp, fp_tmp, e->content->length);
    if (mutt_file_fclose(&fp_tmp) != 0)
    {
      mutt_perror(mutt_b2s(tempfile));
      unlink(mutt_b2s(tempfile));
      return -1;
    }
#ifdef USE_SMTP
    const char *c_smtp_url = cs_subset_string(sub, "smtp_url");
    if (c_smtp_url)
    {
      rc = mutt_smtp_send(env_from, to, NULL, NULL, mutt_b2s(tempfile),
                          (e->content->encoding == ENC_8BIT), sub);
    }
    else
#endif
    {
      rc = mutt_invoke_sendmail(env_from, to, NULL, NULL, mutt_b2s(tempfile),
                                (e->content->encoding == ENC_8BIT), sub);
    }
  }

  mutt_buffer_pool_release(&tempfile);
  return rc;
}

/**
 * mutt_bounce_message - Bounce an email message
 * @param fp  Handle of message
 * @param e   Email
 * @param to  AddressList to bounce to
 * @param sub Config Subset
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_bounce_message(FILE *fp, struct Email *e, struct AddressList *to,
                        struct ConfigSubset *sub)
{
  if (!fp || !e || !to || TAILQ_EMPTY(to))
    return -1;

  const char *fqdn = mutt_fqdn(true, sub);
  char resent_from[256];
  char *err = NULL;

  resent_from[0] = '\0';
  struct Address *from = mutt_default_from(sub);
  struct AddressList from_list = TAILQ_HEAD_INITIALIZER(from_list);
  mutt_addrlist_append(&from_list, from);

  /* mutt_default_from() does not use $realname if the real name is not set
   * in $from, so we add it here.  The reason it is not added in
   * mutt_default_from() is that during normal sending, we execute
   * send-hooks and set the realname last so that it can be changed based
   * upon message criteria.  */
  if (!from->personal)
  {
    const char *c_realname = cs_subset_string(sub, "realname");
    from->personal = mutt_str_dup(c_realname);
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
  mutt_addrlist_write(&from_list, resent_from, sizeof(resent_from), false);

#ifdef USE_NNTP
  OptNewsSend = false;
#endif

  /* prepare recipient list. idna conversion appears to happen before this
   * function is called, since the user receives confirmation of the address
   * list being bounced to.  */
  struct AddressList resent_to = TAILQ_HEAD_INITIALIZER(resent_to);
  mutt_addrlist_copy(&resent_to, to, false);
  rfc2047_encode_addrlist(&resent_to, "Resent-To");
  int rc = bounce_message(fp, e, &resent_to, resent_from, &from_list, sub);
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
  char fcc_tok[PATH_MAX];
  char fcc_expanded[PATH_MAX];

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
  struct stat st;
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
  if ((ctx_fcc->mailbox->type == MUTT_MMDF) || (ctx_fcc->mailbox->type == MUTT_MBOX))
  {
    tempfile = mutt_buffer_pool_get();
    mutt_buffer_mktemp(tempfile);
    fp_tmp = mutt_file_fopen(mutt_b2s(tempfile), "w+");
    if (!fp_tmp)
    {
      mutt_perror(mutt_b2s(tempfile));
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

  const bool c_crypt_protected_headers_read =
      cs_subset_bool(sub, "crypt_protected_headers_read");

  /* post == 1 => postpone message.
   * post == 0 => Normal mode.  */
  mutt_rfc822_write_header(
      msg->fp, e->env, e->content,
      post ? MUTT_WRITE_HEADER_POSTPONE : MUTT_WRITE_HEADER_FCC, false,
      c_crypt_protected_headers_read && mutt_should_hide_protected_subject(e), sub);

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

  if ((ctx_fcc->mailbox->type == MUTT_MMDF) || (ctx_fcc->mailbox->type == MUTT_MBOX))
    fprintf(msg->fp, "Status: RO\n");

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

      const char *c_pgp_sign_as = cs_subset_string(sub, "pgp_sign_as");
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
    fputs("X-Mutt-SMIME: ", msg->fp);
    if (e->security & SEC_ENCRYPT)
    {
      fputc('E', msg->fp);

      const char *c_smime_encrypt_with =
          cs_subset_string(sub, "smime_encrypt_with");
      if (c_smime_encrypt_with)
        fprintf(msg->fp, "C<%s>", c_smime_encrypt_with);
    }
    if (e->security & SEC_OPPENCRYPT)
      fputc('O', msg->fp);
    if (e->security & SEC_SIGN)
    {
      fputc('S', msg->fp);

      const char *c_smime_sign_as = cs_subset_string(sub, "smime_sign_as");
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
    mutt_write_mime_body(e->content, fp_tmp, sub);

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
      mutt_debug(LL_DEBUG1, "%s: write failed\n", mutt_b2s(tempfile));
      mutt_file_fclose(&fp_tmp);
      unlink(mutt_b2s(tempfile));
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
    if (mutt_file_fclose(&fp_tmp) != 0)
      rc = -1;
    /* if there was an error, leave the temp version */
    if (rc >= 0)
    {
      unlink(mutt_b2s(tempfile));
      rc = 0;
    }
  }
  else
  {
    fputc('\n', msg->fp); /* finish off the header */
    rc = mutt_write_mime_body(e->content, msg->fp, sub);
  }

  if (mx_msg_commit(ctx_fcc->mailbox, msg) != 0)
    rc = -1;
  else if (finalpath)
    *finalpath = mutt_str_dup(msg->committed_path);
  mx_msg_close(ctx_fcc->mailbox, &msg);
  mx_mbox_close(&ctx_fcc);

  if (!post && need_mailbox_cleanup)
    mutt_mailbox_cleanup(path, &st);

  if (post)
    set_noconv_flags(e->content, false);

done:
  if (m_fcc)
    m_fcc->append = old_append;
#ifdef RECORD_FOLDER_HOOK
  /* We ran a folder hook for the destination mailbox,
   * now we run it for the user's current mailbox */
  if (Context && Context->mailbox->path)
    mutt_folder_hook(Context->mailbox->path, Context->mailbox->desc);
#endif

  if (fp_tmp)
  {
    mutt_file_fclose(&fp_tmp);
    unlink(mutt_b2s(tempfile));
  }
  mutt_buffer_pool_release(&tempfile);

  return rc;
}
