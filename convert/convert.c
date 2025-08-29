/**
 * @file
 * Conversion between different character encodings
 *
 * @authors
 * Copyright (C) 2022 Michal Siedlaczek <michal@siedlaczek.me>
 * Copyright (C) 2022-2023 Richard Russon <rich@flatcap.org>
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
#include "email/lib.h"
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
  char bufi[256] = { 0 };
  char bufu[512] = { 0 };
  char bufo[4 * sizeof(bufi)] = { 0 };
  size_t rc;

  const iconv_t cd1 = mutt_ch_iconv_open("utf-8", fromcode, MUTT_ICONV_NO_FLAGS);
  if (!iconv_t_valid(cd1))
    return -1;

  int ncodes = tocodes->count;
  iconv_t *cd = MUTT_MEM_CALLOC(ncodes, iconv_t);
  size_t *score = MUTT_MEM_CALLOC(ncodes, size_t);
  struct ContentState *states = MUTT_MEM_CALLOC(ncodes, struct ContentState);
  struct Content *infos = MUTT_MEM_CALLOC(ncodes, struct Content);

  struct ListNode *np = NULL;
  int ni = 0;
  STAILQ_FOREACH(np, &tocodes->head, entries)
  {
    if (!mutt_istr_equal(np->data, "utf-8"))
    {
      cd[ni] = mutt_ch_iconv_open(np->data, "utf-8", MUTT_ICONV_NO_FLAGS);
    }
    else
    {
      /* Special case for conversion to UTF-8 */
      cd[ni] = ICONV_T_INVALID;
      score[ni] = ICONV_ILLEGAL_SEQ;
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
    if ((n == ICONV_ILLEGAL_SEQ) && (((errno != EINVAL) && (errno != E2BIG)) || (ib == bufi)))
    {
      rc = ICONV_ILLEGAL_SEQ;
      break;
    }
    const size_t ubl1 = ob - bufu;

    /* Convert from UTF-8 */
    for (int i = 0; i < ncodes; i++)
    {
      if (iconv_t_valid(cd[i]) && (score[i] != ICONV_ILLEGAL_SEQ))
      {
        const char *ub = bufu;
        size_t ubl = ubl1;
        ob = bufo;
        obl = sizeof(bufo);
        n = iconv(cd[i], (ICONV_CONST char **) ((ibl || ubl) ? &ub : 0), &ubl, &ob, &obl);
        if (n == ICONV_ILLEGAL_SEQ)
        {
          score[i] = ICONV_ILLEGAL_SEQ;
        }
        else
        {
          score[i] += n;
          mutt_update_content_info(&infos[i], &states[i], bufo, ob - bufo);
        }
      }
      else if (!iconv_t_valid(cd[i]) && (score[i] == ICONV_ILLEGAL_SEQ))
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
      rc = 0;
      break;
    }
  }

  if (rc == 0)
  {
    /* Find best score */
    rc = ICONV_ILLEGAL_SEQ;
    for (int i = 0; i < ncodes; i++)
    {
      if (!iconv_t_valid(cd[i]) && (score[i] == ICONV_ILLEGAL_SEQ))
      {
        /* Special case for conversion to UTF-8 */
        *tocode = i;
        rc = 0;
        break;
      }
      else if (!iconv_t_valid(cd[i]) || (score[i] == ICONV_ILLEGAL_SEQ))
      {
        continue;
      }
      else if ((rc == ICONV_ILLEGAL_SEQ) || (score[i] < rc))
      {
        *tocode = i;
        rc = score[i];
        if (rc == 0)
          break;
      }
    }
    if (rc != ICONV_ILLEGAL_SEQ)
    {
      memcpy(info, &infos[*tocode], sizeof(struct Content));
      mutt_update_content_info(info, &states[*tocode], 0, 0); /* EOF */
    }
  }

  FREE(&cd);
  FREE(&infos);
  FREE(&score);
  FREE(&states);

  return rc;
}

/**
 * mutt_convert_file_from_to - Convert a file between encodings
 * @param[in]  fp        File to read from
 * @param[in]  fromcodes Charsets to try converting FROM
 * @param[in]  tocodes   Charsets to try converting TO
 * @param[out] fromcode  From charset selected
 * @param[out] tocode    To charset selected
 * @param[out] info      Info about the file
 * @retval num               Characters converted
 * @retval ICONV_ILLEGAL_SEQ Error (as a size_t)
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
  size_t rc;
  int cn;
  struct ListNode *np = NULL;

  /* Copy them */
  tcode = MUTT_MEM_CALLOC(tocodes->count, char *);
  np = NULL;
  cn = 0;
  STAILQ_FOREACH(np, &tocodes->head, entries)
  {
    tcode[cn++] = mutt_str_dup(np->data);
  }

  rc = ICONV_ILLEGAL_SEQ;
  np = NULL;
  cn = 0;
  STAILQ_FOREACH(np, &fromcodes->head, entries)
  {
    /* Try each fromcode in turn */
    rc = mutt_convert_file_to(fp, np->data, tocodes, &cn, info);
    if (rc != ICONV_ILLEGAL_SEQ)
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

  return rc;
}
