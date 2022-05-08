/**
 * @file
 * Conversion between different character encodings
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
 * @page convert_convert File Charset Conversion
 *
 * Converting files between charsets.
 */

#include "config.h"
#include <errno.h>
#include <iconv.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "email/content.h"
#include "lib.h"

/**
 * mutt_convert_file_to - Change the encoding of a file
 * @param[in]  fp         File to convert
 * @param[in]  fromcode   Original encoding
 * @param[in]  tocodes    List of target encodings
 * @param[out] tocode     Chosen encoding
 * @param[out] info       Encoding information
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
size_t mutt_convert_file_to(FILE *fp, const char *fromcode, struct Slist const *const tocodes,
                            int *tocode, struct Content *info)
{
  char bufi[256], bufu[512], bufo[4 * sizeof(bufi)];
  size_t ret;

  const iconv_t cd1 = mutt_ch_iconv_open("utf-8", fromcode, MUTT_ICONV_NO_FLAGS);
  if (cd1 == (iconv_t) (-1))
    return -1;

  int ncodes = tocodes->count;
  iconv_t *cd = mutt_mem_calloc(ncodes, sizeof(iconv_t));
  size_t *score = mutt_mem_calloc(ncodes, sizeof(size_t));
  struct ContentState *states = mutt_mem_calloc(ncodes, sizeof(struct ContentState));
  struct Content *infos = mutt_mem_calloc(ncodes, sizeof(struct Content));

  struct ListNode *np = NULL;
  int ni = 0;
  STAILQ_FOREACH(np, &tocodes->head, entries)
  {
    if (!mutt_istr_equal(np->data, "utf-8"))
      cd[ni] = mutt_ch_iconv_open(np->data, "utf-8", MUTT_ICONV_NO_FLAGS);
    else
    {
      /* Special case for conversion to UTF-8 */
      cd[ni] = (iconv_t) (-1);
      score[ni] = (size_t) (-1);
    }
    ni += 1;
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
    if ((n == (size_t) (-1)) && (((errno != EINVAL) && (errno != E2BIG)) || (ib == bufi)))
    {
      /* assert(errno == EILSEQ || (errno == EINVAL && ib == bufi && ibl < sizeof(bufi))); */
      ret = (size_t) (-1);
      break;
    }
    const size_t ubl1 = ob - bufu;

    /* Convert from UTF-8 */
    for (int i = 0; i < ncodes; i++)
    {
      if ((cd[i] != (iconv_t) (-1)) && (score[i] != (size_t) (-1)))
      {
        const char *ub = bufu;
        size_t ubl = ubl1;
        ob = bufo;
        obl = sizeof(bufo);
        n = iconv(cd[i], (ICONV_CONST char **) ((ibl || ubl) ? &ub : 0), &ubl, &ob, &obl);
        if (n == (size_t) (-1))
        {
          /* assert(errno == E2BIG || (BUGGY_ICONV && (errno == EILSEQ || errno == ENOENT))); */
          score[i] = (size_t) (-1);
        }
        else
        {
          score[i] += n;
          mutt_update_content_info(&infos[i], &states[i], bufo, ob - bufo);
        }
      }
      else if ((cd[i] == (iconv_t) (-1)) && (score[i] == (size_t) (-1)))
      {
        /* Special case for conversion to UTF-8 */
        mutt_update_content_info(&infos[i], &states[i], bufu, ubl1);
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
    ret = (size_t) (-1);
    for (int i = 0; i < ncodes; i++)
    {
      if ((cd[i] == (iconv_t) (-1)) && (score[i] == (size_t) (-1)))
      {
        /* Special case for conversion to UTF-8 */
        *tocode = i;
        ret = 0;
        break;
      }
      else if ((cd[i] == (iconv_t) (-1)) || (score[i] == (size_t) (-1)))
        continue;
      else if ((ret == (size_t) (-1)) || (score[i] < ret))
      {
        *tocode = i;
        ret = score[i];
        if (ret == 0)
          break;
      }
    }
    if (ret != (size_t) (-1))
    {
      memcpy(info, &infos[*tocode], sizeof(struct Content));
      mutt_update_content_info(info, &states[*tocode], 0, 0); /* EOF */
    }
  }

  for (int i = 0; i < ncodes; i++)
    if (cd[i] != (iconv_t) (-1))
      iconv_close(cd[i]);

  iconv_close(cd1);
  FREE(&cd);
  FREE(&infos);
  FREE(&score);
  FREE(&states);

  return ret;
}

/**
 * mutt_convert_file_from_to - Convert a file between encodings
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
 */
size_t mutt_convert_file_from_to(FILE *fp, const struct Slist *fromcodes,
                                 const struct Slist *tocodes, char **fromcode,
                                 char **tocode, struct Content *info)
{
  char **tcode = NULL;
  size_t ret;
  int cn;
  struct ListNode *np = NULL;

  /* Copy them */
  tcode = mutt_mem_malloc(tocodes->count * sizeof(char *));
  np = NULL;
  cn = 0;
  STAILQ_FOREACH(np, &tocodes->head, entries)
  {
    tcode[cn++] = mutt_str_dup(np->data);
  }

  ret = (size_t) (-1);
  np = NULL;
  cn = 0;
  STAILQ_FOREACH(np, &fromcodes->head, entries)
  {
    /* Try each fromcode in turn */
    ret = mutt_convert_file_to(fp, np->data, tocodes, &cn, info);
    if (ret != (size_t) (-1))
    {
      *fromcode = np->data;
      *tocode = tcode[cn];
      tcode[cn] = 0;
      break;
    }
  }

  /* Free memory */
  for (cn = 0; cn < tocodes->count; cn++)
    FREE(&tcode[cn]);

  FREE(&tcode);

  return ret;
}
