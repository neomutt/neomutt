/**
 * @file
 * Extracting content info from email body.
 *
 * @authors
 * Copyright (C) 2022 Michal Siedlaczek <michal@siedlaczek.me>
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
 * @page convert_content_info Content Info Extraction
 *
 * Extracting content info from email body.
 */

#include "config.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "lib.h"

/**
 * mutt_update_content_info - Cache some info about an email
 * @param info   Info about an Attachment
 * @param s      Info about the Body of an email
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 */
void mutt_update_content_info(struct Content *info, struct ContentState *s,
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
    {
      info->hibin++;
    }
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
    {
      info->lobin++;
    }
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
        {
          from = false;
        }
        else if ((linelen == 3) && (ch != 'o'))
        {
          from = false;
        }
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
  struct ContentState cstate = { 0 };
  FILE *fp = NULL;
  char *fromcode = NULL;
  char *tocode = NULL;
  char buf[100] = { 0 };
  size_t r;

  struct stat st = { 0 };

  if (b && !fname)
    fname = b->filename;
  if (!fname)
    return NULL;

  if (stat(fname, &st) == -1)
  {
    mutt_error(_("Can't stat %s: %s"), fname, strerror(errno));
    return NULL;
  }

  if (!S_ISREG(st.st_mode))
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

  const char *const c_charset = cc_charset();
  if (b && (b->type == TYPE_TEXT) && (!b->noconv && !b->force_charset))
  {
    const struct Slist *const c_attach_charset = cs_subset_slist(sub, "attach_charset");
    const struct Slist *const c_send_charset = cs_subset_slist(sub, "send_charset");
    struct Slist *c_charset_slist = slist_parse(c_charset, SLIST_SEP_COLON);

    const struct Slist *fchs = b->use_disp ?
                                   (c_attach_charset ? c_attach_charset : c_charset_slist) :
                                   c_charset_slist;

    struct Slist *chs = slist_parse(mutt_param_get(&b->parameter, "charset"), SLIST_SEP_COLON);

    if (c_charset && (chs || c_send_charset) &&
        (mutt_convert_file_from_to(fp, fchs, chs ? chs : c_send_charset, &fromcode,
                                   &tocode, info) != ICONV_ILLEGAL_SEQ))
    {
      if (!chs)
      {
        char chsbuf[256] = { 0 };
        mutt_ch_canonical_charset(chsbuf, sizeof(chsbuf), tocode);
        mutt_param_set(&b->parameter, "charset", chsbuf);
      }
      FREE(&b->charset);
      b->charset = mutt_str_dup(fromcode);
      FREE(&tocode);
      mutt_file_fclose(&fp);
      slist_free(&c_charset_slist);
      slist_free(&chs);
      return info;
    }

    slist_free(&c_charset_slist);
    slist_free(&chs);
  }

  rewind(fp);
  while ((r = fread(buf, 1, sizeof(buf), fp)))
    mutt_update_content_info(info, &cstate, buf, r);
  mutt_update_content_info(info, &cstate, 0, 0);

  mutt_file_fclose(&fp);

  if (b && (b->type == TYPE_TEXT) && (!b->noconv && !b->force_charset))
  {
    mutt_param_set(&b->parameter, "charset",
                   (!info->hibin                                 ? "us-ascii" :
                    c_charset && !mutt_ch_is_us_ascii(c_charset) ? c_charset :
                                                                   "unknown-8bit"));
  }

  return info;
}
