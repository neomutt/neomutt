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
    if (!strstr ("-_.", *s))
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
    snprintf (buff2, sizeof (buff2), "windows-%s" buff + 7);
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


/*************************************************************
 * General decoder framework
 * Used in handler.c for converting to mutt's Charset
 */

#define MIN(a,b) (((a) <= (b)) ? (a): (b))

DECODER *mutt_open_decoder (const char *src, const char *dest)
{
  DECODER *d = safe_calloc (1, sizeof (DECODER));;

  d->in.size = DECODER_BUFFSIZE;
  d->out.size = DECODER_BUFFSIZE;

  if (dest && src && (d->cd = mutt_iconv_open (dest, src)) != (iconv_t)-1)
  {
    d->_in = &d->in;
    d->outrepl = mutt_is_utf8 (dest) ? "\357\277\275" : "?";
  }
  else
  {
    d->just_take_id = 1;
    d->_in = &d->out;
  }
  return d;
}

void mutt_free_decoder (DECODER **dpp)
{
  safe_free ((void **) dpp);
}

static void _process_data (DECODER *d, short force)
{
  if (force) d->forced = 1;
  
  if (!d->just_take_id)
  {
    const char *ib = d->in.buff;
    size_t ibl = d->in.used;
    char *ob = d->out.buff + d->out.used;
    size_t obl = d->out.size - d->out.used;

    mutt_iconv (d->cd, &ib, &ibl, &ob, &obl, 0, d->outrepl);
    memmove (d->in.buff, ib, ibl);
    d->in.used = ibl;
    d->out.used = d->out.size - obl;
  }
}

void mutt_decoder_push (DECODER *d, void *_buff, size_t blen, size_t *taken)
{
  if (!_buff || !blen)
  {
    _process_data (d, 1);
    return;
  }

  if ((*taken = MIN(blen, d->_in->size - d->_in->used)))
  {
    memcpy (d->_in->buff + d->_in->used, _buff, *taken);
    d->_in->used += *taken;
  }
}

int mutt_decoder_push_one (DECODER *d, char c)
{
  if (d->_in->used == d->_in->size)
    return -1;

  d->_in->buff[d->_in->used++] = c;
  return 0;
}

void mutt_decoder_pop (DECODER *d, void *_buff, size_t blen, size_t *popped)
{
  unsigned char *buff = _buff;

  _process_data (d, 0);
  
  if ((*popped = MIN (blen, d->out.used)))
  {
    memcpy (buff, d->out.buff, *popped);
    memmove (d->out.buff, d->out.buff + *popped, d->out.used - *popped);
    d->out.used -= *popped;
  }
}

void mutt_decoder_pop_to_state (DECODER *d, STATE *s)
{
  char tmp[DECODER_BUFFSIZE];
  size_t i, l;

  if (s->prefix)
  {
    do 
    {
      mutt_decoder_pop (d, tmp, sizeof (tmp), &l);
      for (i = 0; i < l; i++)
	state_prefix_putc (tmp[i], s);
    }
    while (l > 0);
  }
  else
  {
    do 
    {
      mutt_decoder_pop (d, tmp, sizeof (tmp), &l);
      fwrite (tmp, l, 1, s->fpout);
    }
    while (l > 0);
  }
}


int mutt_recode_file (const char *fname, const char *src, const char *dest)
{
  FILE *fp, *tmpfp;
  char tempfile[_POSIX_PATH_MAX];
  char buffer[1024];
  char tmp[1024];
  int c;
  int rv = -1;
  int source_file_is_unchanged = 1;

  size_t lf, lpu, lpo;
  char *t;
  DECODER *dec;

  if ((fp = fopen (fname, "r+")) == NULL)
  {
    mutt_error (_("Can't open %s: %s."), fname, strerror (errno));
    return -1;
  }
  
  mutt_mktemp (tempfile);
  if ((tmpfp = safe_fopen (tempfile, "w+")) == NULL)
  {
    mutt_error (_("Can't open %s: %s."), tempfile, strerror (errno));
    fclose (fp);
    return -1;
  }

  dec = mutt_open_decoder (src, dest);
  
  while ((lf = fread (buffer, 1, sizeof (buffer), fp)) > 0)
  {
    for (t = buffer; lf; t += lpu)
    {
      mutt_decoder_push (dec, t, lf, &lpu);
      lf -= lpu;
      
      do
      {
	mutt_decoder_pop (dec, tmp, sizeof (tmp), &lpo);
	if (lpo)
	{
	  if (fwrite (tmp, lpo, 1, tmpfp) == EOF)
	    goto bail;
	}
      } 
      while (lpo);
    }
  }
  if (lf == EOF && !feof(fp))
  {
    goto bail;
  }

  mutt_decoder_push (dec, NULL, 0, NULL);
  do 
  {
    mutt_decoder_pop (dec, tmp, sizeof (tmp), &lpo);
    if (lpo)
    {
      if (fwrite (tmp, lpo, 1, tmpfp) == EOF)
	goto bail;
    }
  }
  while (lpo);

  mutt_free_decoder (&dec);

  fclose (fp); fp = NULL;
  rewind (tmpfp);


  source_file_is_unchanged = 0;

  /* don't use safe_fopen here - we're just going
   * to overwrite the old file.
   */
  if ((fp = fopen (fname, "w")) == NULL)
    goto bail;
  
  while ((c = fgetc (tmpfp)) != EOF)
    if (fputc (c, fp) == EOF)
      goto bail;

  rv = 0;
  unlink (tempfile);
  
bail:
  if (rv == -1)
  {
    if (source_file_is_unchanged)
    {
      mutt_error (_("Error while recoding %s. "
		    "Leave it unchanged."),
		  fname);
    }
    else
    {
      mutt_error (_("Error while recoding %s. "
		    "See %s for recovering your data."),
		  fname, tempfile);
    }
  }

  if (fp) fclose (fp);
  if (tmpfp) fclose (tmpfp);
  return rv;
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
