/*
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@guug.de>
 *
 *     This program is free software; you can redistribute it
 *     and/or modify it under the terms of the GNU General Public
 *     License as published by the Free Software Foundation; either
 *     version 2 of the License, or (at your option) any later
 *     version.
 *
 *     This program is distributed in the hope that it will be
 *     useful, but WITHOUT ANY WARRANTY; without even the implied
 *     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *     PURPOSE.  See the GNU General Public License for more
 *     details.
 *
 *     You should have received a copy of the GNU General Public
 *     License along with this program; if not, write to the Free
 *     Software Foundation, Inc., 59 Temple Place - Suite 330,
 *     Boston, MA  02111, USA.
 */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#include <iconv.h>

#include "mutt.h"
#include "charset.h"

#ifndef EILSEQ
# define EILSEQ EINVAL
#endif

#ifdef HAVE_LANGINFO_CODESET
# include <langinfo.h>

/* 
 * Try to convert nl_langinfo's return value to something we can
 * use for MIME's purposes.
 *
 * Note that the algorithm used here is quite different from the
 * one in mutt_canonical_charset. 
 */

void mutt_set_langinfo_charset (void)
{
  char buff[LONG_STRING];
  char buff2[LONG_STRING];
  char *s, *d, *cp;
  
  strfcpy (buff, nl_langinfo (CODESET), sizeof (buff));
  strfcpy (buff2, buff, sizeof (buff2));
  
  /* compactify the character set name returned */
  for (d = s = buff; *s; s++)
  {
    if (!strchr ("-_.", *s))
      *d++ = *s;
  }
  *d = '\0';
  
  /* look for common prefixes which may have been done wrong */
  if (!strncasecmp (buff, "iso8859", 7))
  {
    snprintf (buff2, sizeof (buff2), "iso-8859-%s", buff + 7);
    if ((cp = strchr (buff2, ':')))	/* strip :yyyy suffixes */
      *cp = '\0';
  }
  else if (!strncasecmp (buff, "koi8", 4))
  {
    snprintf (buff2, sizeof (buff2), "koi8-%s", buff + 4);
  }
  else if (!strncasecmp (buff, "windows", 7))
  {
    snprintf (buff2, sizeof (buff2), "windows-%s", buff + 7);
  }

  /* fix the spelling */
  mutt_canonical_charset (buff, sizeof (buff), buff2);
  
  /* finally, set $charset */
  Charset = safe_strdup (buff);
}

#endif

void mutt_canonical_charset (char *dest, size_t dlen, const char *name)
{
  size_t i;

  if (!strncasecmp (name, "x-", 2))
    name = name + 2;

  for (i = 0; name[i] && i < dlen - 1; i++)
  {
    if (strchr ("_/. ", name[i]))
      dest[i] = '-';
    else if ('A' <= name[i] && name[i] <= 'Z')
      dest[i] = name[i] - 'A' + 'a';
    else
      dest[i] = name[i];
  }

  dest[i] = '\0';
}

int mutt_is_utf8 (const char *s)
{
  char buffer[8];

  if (!s) 
    return 0;

  mutt_canonical_charset (buffer, sizeof (buffer), s);
  return !mutt_strcmp (buffer, "utf-8");
}


/*
 * Like iconv_open, but canonicalises the charsets
 */

iconv_t mutt_iconv_open (const char *tocode, const char *fromcode)
{
  char tocode1[SHORT_STRING];
  char fromcode1[SHORT_STRING];
  char *tmp;

  mutt_canonical_charset (tocode1, sizeof (tocode1), tocode);
  if ((tmp = mutt_charset_hook (tocode1)))
    mutt_canonical_charset (tocode1, sizeof (tocode1), tmp);

  mutt_canonical_charset (fromcode1, sizeof (fromcode1), fromcode);
  if ((tmp = mutt_charset_hook (fromcode1)))
    mutt_canonical_charset (fromcode1, sizeof (fromcode1), tmp);

  return iconv_open (tocode1, fromcode1);
}


/*
 * Like iconv, but keeps going even when the input is invalid
 * If you're supplying inrepls, the source charset should be stateless;
 * if you're supplying an outrepl, the target charset should be.
 */

size_t mutt_iconv (iconv_t cd, const char **inbuf, size_t *inbytesleft,
		   char **outbuf, size_t *outbytesleft,
		   const char **inrepls, const char *outrepl)
{
  size_t ret = 0, ret1;
  const char *ib = *inbuf;
  size_t ibl = *inbytesleft;
  char *ob = *outbuf;
  size_t obl = *outbytesleft;

  for (;;)
  {
    ret1 = iconv (cd, &ib, &ibl, &ob, &obl);
    if (ret1 != (size_t)-1)
      ret += ret1;
    if (ibl && obl && errno == EILSEQ)
    {
      if (inrepls)
      {
	/* Try replacing the input */
	const char **t;
	for (t = inrepls; *t; t++)
	{
	  const char *ib1 = *t;
	  size_t ibl1 = strlen (*t);
	  char *ob1 = ob;
	  size_t obl1 = obl;
	  iconv (cd, &ib1, &ibl1, &ob1, &obl1);
	  if (!ibl1)
	  {
	    ++ib, --ibl;
	    ob = ob1, obl = obl1;
	    ++ret;
	    break;
	  }
	}
	if (*t)
	  continue;
      }
      if (outrepl)
      {
	/* Try replacing the output */
	int n = strlen (outrepl);
	if (n <= obl)
	{
	  memcpy (ob, outrepl, n);
	  ++ib, --ibl;
	  ob += n, obl -= n;
	  ++ret;
	  continue;
	}
      }
    }
    *inbuf = ib, *inbytesleft = ibl;
    *outbuf = ob, *outbytesleft = obl;
    return ret;
  }
}


/*
 * Convert a string
 * Used in rfc2047.c and rfc2231.c
 */

int mutt_convert_string (char **ps, const char *from, const char *to)
{
  iconv_t cd;
  const char *repls[] = { "\357\277\275", "?", 0 };
  char *s = *ps;

  if (!s || !*s)
    return 0;

  if (to && from && (cd = mutt_iconv_open (to, from)) != (iconv_t)-1) 
  {
    int len;
    const char *ib;
    char *buf, *ob;
    size_t ibl, obl;
    const char **inrepls = 0;
    char *outrepl = 0;

    if (mutt_is_utf8 (to))
      outrepl = "\357\277\275";
    else if (mutt_is_utf8 (from))
      inrepls = repls;
    else
      outrepl = "?";
      
    len = strlen (s);
    ib = s, ibl = len + 1;
    obl = MB_LEN_MAX * ibl;
    ob = buf = safe_malloc (obl + 1);
    
    mutt_iconv (cd, &ib, &ibl, &ob, &obl, inrepls, outrepl);
    iconv_close (cd);

    *ob = '\0';

    safe_free ((void **) ps);
    *ps = buf;
    
    mutt_str_adjust (ps);
    return 0;
  }
  else
    return -1;
}


/*
 * FGETCONV stuff for converting a file while reading it
 * Used in sendlib.c for converting from mutt's Charset
 */

struct fgetconv_s
{
  FILE *file;
  iconv_t cd;
  char bufi[512];
  char bufo[512];
  char *p;
  char *ob;
  char *ib;
  size_t ibl;
  const char **inrepls;
};

struct fgetconv_not
{
  FILE *file;
  iconv_t cd;
};

FGETCONV *fgetconv_open (FILE *file, const char *from, const char *to)
{
  struct fgetconv_s *fc;
  iconv_t cd = (iconv_t)-1;
  static const char *repls[] = { "\357\277\275", "?", 0 };

  if (from && to)
    cd = mutt_iconv_open (to, from);

  if (cd != (iconv_t)-1)
  {
    fc = safe_malloc (sizeof (struct fgetconv_s));
    fc->p = fc->ob = fc->bufo;
    fc->ib = fc->bufi;
    fc->ibl = 0;
    fc->inrepls = mutt_is_utf8 (to) ? repls : repls + 1;
  }
  else
    fc = safe_malloc (sizeof (struct fgetconv_not));
  fc->file = file;
  fc->cd = cd;
  return (FGETCONV *)fc;
}

int fgetconv (FGETCONV *_fc)
{
  struct fgetconv_s *fc = (struct fgetconv_s *)_fc;

  if (!fc)
    return EOF;
  if (fc->cd == (iconv_t)-1)
    return fgetc (fc->file);
  if (!fc->p)
    return EOF;
  if (fc->p < fc->ob)
    return (unsigned char)*(fc->p)++;

  /* Try to convert some more */
  fc->p = fc->ob = fc->bufo;
  if (fc->ibl)
  {
    size_t obl = sizeof (fc->bufo);
    iconv (fc->cd, (const char **)&fc->ib, &fc->ibl, &fc->ob, &obl);
    if (fc->p < fc->ob)
      return (unsigned char)*(fc->p)++;
  }

  /* If we trusted iconv a bit more, we would at this point
   * ask why it had stopped converting ... */

  /* Try to read some more */
  if (fc->ibl == sizeof (fc->bufi) ||
      (fc->ibl && fc->ib + fc->ibl < fc->bufi + sizeof (fc->bufi)))
  {
    fc->p = 0;
    return EOF;
  }
  if (fc->ibl)
    memcpy (fc->bufi, fc->ib, fc->ibl);
  fc->ib = fc->bufi;
  fc->ibl += fread (fc->ib + fc->ibl, 1, sizeof (fc->bufi) - fc->ibl, fc->file);

  /* Try harder this time to convert some */
  if (fc->ibl)
  {
    size_t obl = sizeof (fc->bufo);
    mutt_iconv (fc->cd, (const char **)&fc->ib, &fc->ibl, &fc->ob, &obl,
		fc->inrepls, 0);
    if (fc->p < fc->ob)
      return (unsigned char)*(fc->p)++;
  }

  /* Either the file has finished or one of the buffers is too small */
  fc->p = 0;
  return EOF;
}

void fgetconv_close (FGETCONV *_fc)
{
  struct fgetconv_s *fc = (struct fgetconv_s *)_fc;

  if (fc->cd != (iconv_t)-1)
    iconv_close (fc->cd);
  free (fc);
}
