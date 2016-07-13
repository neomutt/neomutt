/*
 * Copyright (C) 1996-2002,2009-2012 Michael R. Elkins <me@mutt.org>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define _SENDLIB_C 1

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "rfc2047.h"
#include "rfc2231.h"
#include "mx.h"
#include "mime.h"
#include "mailbox.h"
#include "copy.h"
#include "pager.h"
#include "charset.h"
#include "mutt_crypt.h"
#include "mutt_idna.h"
#include "buffy.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#else /* Make sure EX_OK is defined <philiph@pobox.com> */
#define EX_OK 0
#endif

/* If you are debugging this file, comment out the following line. */
#define NDEBUG

#ifdef NDEBUG
#define assert(x)
#else
#include <assert.h>
#endif

extern char RFC822Specials[];

const char MimeSpecials[] = "@.,;:<>[]\\\"()?/= \t";

const char B64Chars[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
  'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
  'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
  't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', '+', '/'
};

static char MsgIdPfx = 'A';

static void transform_to_7bit (BODY *a, FILE *fpin);

static void encode_quoted (FGETCONV * fc, FILE *fout, int istext)
{
  int c, linelen = 0;
  char line[77], savechar;

  while ((c = fgetconv (fc)) != EOF)
  {
    /* Wrap the line if needed. */
    if (linelen == 76 && ((istext && c != '\n') || !istext))
    {
      /* If the last character is "quoted", then be sure to move all three
       * characters to the next line.  Otherwise, just move the last
       * character...
       */
      if (line[linelen-3] == '=')
      {
        line[linelen-3] = 0;
        fputs (line, fout);
        fputs ("=\n", fout);
        line[linelen] = 0;
        line[0] = '=';
        line[1] = line[linelen-2];
        line[2] = line[linelen-1];
        linelen = 3;
      }
      else
      {
        savechar = line[linelen-1];
        line[linelen-1] = '=';
        line[linelen] = 0;
        fputs (line, fout);
        fputc ('\n', fout);
        line[0] = savechar;
        linelen = 1;
      }
    }

    /* Escape lines that begin with/only contain "the message separator". */
    if (linelen == 4 && !mutt_strncmp ("From", line, 4))
    {
      strfcpy (line, "=46rom", sizeof (line));
      linelen = 6;
    }
    else if (linelen == 4 && !mutt_strncmp ("from", line, 4))
    {
      strfcpy (line, "=66rom", sizeof (line));
      linelen = 6;
    }
    else if (linelen == 1 && line[0] == '.')
    {
      strfcpy (line, "=2E", sizeof (line));
      linelen = 3;
    }


    if (c == '\n' && istext)
    {
      /* Check to make sure there is no trailing space on this line. */
      if (linelen > 0 && (line[linelen-1] == ' ' || line[linelen-1] == '\t'))
      {
        if (linelen < 74)
	{
          sprintf (line+linelen-1, "=%2.2X", (unsigned char) line[linelen-1]);
          fputs (line, fout);
        }
        else
	{
          int savechar = line[linelen-1];

          line[linelen-1] = '=';
          line[linelen] = 0;
          fputs (line, fout);
          fprintf (fout, "\n=%2.2X", (unsigned char) savechar);
        }
      }
      else
      {
        line[linelen] = 0;
        fputs (line, fout);
      }
      fputc ('\n', fout);
      linelen = 0;
    }
    else if (c != 9 && (c < 32 || c > 126 || c == '='))
    {
      /* Check to make sure there is enough room for the quoted character.
       * If not, wrap to the next line.
       */
      if (linelen > 73)
      {
        line[linelen++] = '=';
        line[linelen] = 0;
        fputs (line, fout);
        fputc ('\n', fout);
        linelen = 0;
      }
      sprintf (line+linelen,"=%2.2X", (unsigned char) c);
      linelen += 3;
    }
    else
    {
      /* Don't worry about wrapping the line here.  That will happen during
       * the next iteration when I'll also know what the next character is.
       */
      line[linelen++] = c;
    }
  }

  /* Take care of anything left in the buffer */
  if (linelen > 0)
  {
    if (line[linelen-1] == ' ' || line[linelen-1] == '\t')
    {
      /* take care of trailing whitespace */
      if (linelen < 74)
        sprintf (line+linelen-1, "=%2.2X", (unsigned char) line[linelen-1]);
      else
      {
        savechar = line[linelen-1];
        line[linelen-1] = '=';
        line[linelen] = 0;
        fputs (line, fout);
        fputc ('\n', fout);
        sprintf (line, "=%2.2X", (unsigned char) savechar);
      }
    }
    else
      line[linelen] = 0;
    fputs (line, fout);
  }
}

static char b64_buffer[3];
static short b64_num;
static short b64_linelen;

static void b64_flush(FILE *fout)
{
  short i;

  if(!b64_num)
    return;

  if(b64_linelen >= 72)
  {
    fputc('\n', fout);
    b64_linelen = 0;
  }

  for(i = b64_num; i < 3; i++)
    b64_buffer[i] = '\0';

  fputc(B64Chars[(b64_buffer[0] >> 2) & 0x3f], fout);
  b64_linelen++;
  fputc(B64Chars[((b64_buffer[0] & 0x3) << 4) | ((b64_buffer[1] >> 4) & 0xf) ], fout);
  b64_linelen++;

  if(b64_num > 1)
  {
    fputc(B64Chars[((b64_buffer[1] & 0xf) << 2) | ((b64_buffer[2] >> 6) & 0x3) ], fout);
    b64_linelen++;
    if(b64_num > 2)
    {
      fputc(B64Chars[b64_buffer[2] & 0x3f], fout);
      b64_linelen++;
    }
  }

  while(b64_linelen % 4)
  {
    fputc('=', fout);
    b64_linelen++;
  }

  b64_num = 0;
}


static void b64_putc(char c, FILE *fout)
{
  if(b64_num == 3)
    b64_flush(fout);

  b64_buffer[b64_num++] = c;
}


static void encode_base64 (FGETCONV * fc, FILE *fout, int istext)
{
  int ch, ch1 = EOF;

  b64_num = b64_linelen = 0;

  while ((ch = fgetconv (fc)) != EOF)
  {
    if (istext && ch == '\n' && ch1 != '\r')
      b64_putc('\r', fout);
    b64_putc(ch, fout);
    ch1 = ch;
  }
  b64_flush(fout);
  fputc('\n', fout);
}

static void encode_8bit (FGETCONV *fc, FILE *fout, int istext)
{
  int ch;

  while ((ch = fgetconv (fc)) != EOF)
    fputc (ch, fout);
}


int mutt_write_mime_header (BODY *a, FILE *f)
{
  PARAMETER *p;
  char buffer[STRING];
  char *t;
  char *fn;
  int len;
  int tmplen;
  int encode;

  fprintf (f, "Content-Type: %s/%s", TYPE (a), a->subtype);

  if (a->parameter)
  {
    len = 25 + mutt_strlen (a->subtype); /* approximate len. of content-type */

    for(p = a->parameter; p; p = p->next)
    {
      char *tmp;

      if(!p->value)
	continue;

      fputc (';', f);

      buffer[0] = 0;
      tmp = safe_strdup (p->value);
      encode = rfc2231_encode_string (&tmp);
      rfc822_cat (buffer, sizeof (buffer), tmp, MimeSpecials);

      /* Dirty hack to make messages readable by Outlook Express
       * for the Mac: force quotes around the boundary parameter
       * even when they aren't needed.
       */

      if (!ascii_strcasecmp (p->attribute, "boundary") && !strcmp (buffer, tmp))
	snprintf (buffer, sizeof (buffer), "\"%s\"", tmp);

      FREE (&tmp);

      tmplen = mutt_strlen (buffer) + mutt_strlen (p->attribute) + 1;

      if (len + tmplen + 2 > 76)
      {
	fputs ("\n\t", f);
	len = tmplen + 8;
      }
      else
      {
	fputc (' ', f);
	len += tmplen + 1;
      }

      fprintf (f, "%s%s=%s", p->attribute, encode ? "*" : "", buffer);

    }
  }

  fputc ('\n', f);

  if (a->description)
    fprintf(f, "Content-Description: %s\n", a->description);

  if (a->disposition != DISPNONE)
  {
    const char *dispstr[] = {
      "inline",
      "attachment",
      "form-data"
    };

    if (a->disposition < sizeof(dispstr)/sizeof(char*))
    {
      fprintf (f, "Content-Disposition: %s", dispstr[a->disposition]);

      if (a->use_disp)
      {
	if (!(fn = a->d_filename))
	  fn = a->filename;

	if (fn)
	{
	  char *tmp;

	  /* Strip off the leading path... */
	  if ((t = strrchr (fn, '/')))
	    t++;
	  else
	    t = fn;

	  buffer[0] = 0;
	  tmp = safe_strdup (t);
	  encode = rfc2231_encode_string (&tmp);
	  rfc822_cat (buffer, sizeof (buffer), tmp, MimeSpecials);
	  FREE (&tmp);
	  fprintf (f, "; filename%s=%s", encode ? "*" : "", buffer);
	}
      }

      fputc ('\n', f);
    }
    else
    {
      dprint(1, (debugfile, "ERROR: invalid content-disposition %d\n", a->disposition));
    }
  }

  if (a->encoding != ENC7BIT)
    fprintf(f, "Content-Transfer-Encoding: %s\n", ENCODING (a->encoding));

  /* Do NOT add the terminator here!!! */
  return (ferror (f) ? -1 : 0);
}

# define write_as_text_part(a)  (mutt_is_text_part(a) \
                                 || ((WithCrypto & APPLICATION_PGP)\
                                      && mutt_is_application_pgp(a)))

int mutt_write_mime_body (BODY *a, FILE *f)
{
  char *p, boundary[SHORT_STRING];
  char send_charset[SHORT_STRING];
  FILE *fpin;
  BODY *t;
  FGETCONV *fc;

  if (a->type == TYPEMULTIPART)
  {
    /* First, find the boundary to use */
    if (!(p = mutt_get_parameter ("boundary", a->parameter)))
    {
      dprint (1, (debugfile, "mutt_write_mime_body(): no boundary parameter found!\n"));
      mutt_error _("No boundary parameter found! [report this error]");
      return (-1);
    }
    strfcpy (boundary, p, sizeof (boundary));

    for (t = a->parts; t ; t = t->next)
    {
      fprintf (f, "\n--%s\n", boundary);
      if (mutt_write_mime_header (t, f) == -1)
	return -1;
      fputc ('\n', f);
      if (mutt_write_mime_body (t, f) == -1)
	return -1;
    }
    fprintf (f, "\n--%s--\n", boundary);
    return (ferror (f) ? -1 : 0);
  }

  /* This is pretty gross, but it's the best solution for now... */
  if ((WithCrypto & APPLICATION_PGP)
      && a->type == TYPEAPPLICATION
      && mutt_strcmp (a->subtype, "pgp-encrypted") == 0)
  {
    fputs ("Version: 1\n", f);
    return 0;
  }

  if ((fpin = fopen (a->filename, "r")) == NULL)
  {
    dprint(1,(debugfile, "write_mime_body: %s no longer exists!\n",a->filename));
    mutt_error (_("%s no longer exists!"), a->filename);
    return -1;
  }

  if (a->type == TYPETEXT && (!a->noconv))
    fc = fgetconv_open (fpin, a->charset,
			mutt_get_body_charset (send_charset, sizeof (send_charset), a),
			0);
  else
    fc = fgetconv_open (fpin, 0, 0, 0);

  if (a->encoding == ENCQUOTEDPRINTABLE)
    encode_quoted (fc, f, write_as_text_part (a));
  else if (a->encoding == ENCBASE64)
    encode_base64 (fc, f, write_as_text_part (a));
  else if (a->type == TYPETEXT && (!a->noconv))
    encode_8bit (fc, f, write_as_text_part (a));
  else
    mutt_copy_stream (fpin, f);

  fgetconv_close (&fc);
  safe_fclose (&fpin);

  return (ferror (f) ? -1 : 0);
}

#undef write_as_text_part

#define BOUNDARYLEN 16
void mutt_generate_boundary (PARAMETER **parm)
{
  char rs[BOUNDARYLEN + 1];
  char *p = rs;
  int i;

  rs[BOUNDARYLEN] = 0;
  for (i=0;i<BOUNDARYLEN;i++)
    *p++ = B64Chars[LRAND() % sizeof (B64Chars)];
  *p = 0;

  mutt_set_parameter ("boundary", rs, parm);
}

typedef struct
{
  int from;
  int whitespace;
  int dot;
  int linelen;
  int was_cr;
}
CONTENT_STATE;


static void update_content_info (CONTENT *info, CONTENT_STATE *s, char *d, size_t dlen)
{
  int from = s->from;
  int whitespace = s->whitespace;
  int dot = s->dot;
  int linelen = s->linelen;
  int was_cr = s->was_cr;

  if (!d) /* This signals EOF */
  {
    if (was_cr)
      info->binary = 1;
    if (linelen > info->linemax)
      info->linemax = linelen;

    return;
  }

  for (; dlen; d++, dlen--)
  {
    char ch = *d;

    if (was_cr)
    {
      was_cr = 0;
      if (ch != '\n')
      {
        info->binary = 1;
      }
      else
      {
        if (whitespace) info->space = 1;
	if (dot) info->dot = 1;
        if (linelen > info->linemax) info->linemax = linelen;
        whitespace = 0;
	dot = 0;
        linelen = 0;
	continue;
      }
    }

    linelen++;
    if (ch == '\n')
    {
      info->crlf++;
      if (whitespace) info->space = 1;
      if (dot) info->dot = 1;
      if (linelen > info->linemax) info->linemax = linelen;
      whitespace = 0;
      linelen = 0;
      dot = 0;
    }
    else if (ch == '\r')
    {
      info->crlf++;
      info->cr = 1;
      was_cr = 1;
      continue;
    }
    else if (ch & 0x80)
      info->hibin++;
    else if (ch == '\t' || ch == '\f')
    {
      info->ascii++;
      whitespace++;
    }
    else if (ch < 32 || ch == 127)
      info->lobin++;
    else
    {
      if (linelen == 1)
      {
        if ((ch == 'F') || (ch == 'f'))
          from = 1;
        else
          from = 0;
        if (ch == '.')
          dot = 1;
        else
          dot = 0;
      }
      else if (from)
      {
        if (linelen == 2 && ch != 'r') from = 0;
        else if (linelen == 3 && ch != 'o') from = 0;
        else if (linelen == 4)
	{
          if (ch == 'm') info->from = 1;
          from = 0;
        }
      }
      if (ch == ' ') whitespace++;
      info->ascii++;
    }

    if (linelen > 1) dot = 0;
    if (ch != ' ' && ch != '\t') whitespace = 0;
  }

  s->from = from;
  s->whitespace = whitespace;
  s->dot = dot;
  s->linelen = linelen;
  s->was_cr = was_cr;

}

/* Define as 1 if iconv sometimes returns -1(EILSEQ) instead of transcribing. */
#define BUGGY_ICONV 1

/*
 * Find the best charset conversion of the file from fromcode into one
 * of the tocodes. If successful, set *tocode and CONTENT *info and
 * return the number of characters converted inexactly. If no
 * conversion was possible, return -1.
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
static size_t convert_file_to (FILE *file, const char *fromcode,
			       int ncodes, const char **tocodes,
			       int *tocode, CONTENT *info)
{
#ifdef HAVE_ICONV
  iconv_t cd1, *cd;
  char bufi[256], bufu[512], bufo[4 * sizeof (bufi)];
  ICONV_CONST char *ib, *ub;
  char *ob;
  size_t ibl, obl, ubl, ubl1, n, ret;
  int i;
  CONTENT *infos;
  CONTENT_STATE *states;
  size_t *score;

  cd1 = mutt_iconv_open ("utf-8", fromcode, 0);
  if (cd1 == (iconv_t)(-1))
    return -1;

  cd     = safe_calloc (ncodes, sizeof (iconv_t));
  score  = safe_calloc (ncodes, sizeof (size_t));
  states = safe_calloc (ncodes, sizeof (CONTENT_STATE));
  infos  = safe_calloc (ncodes, sizeof (CONTENT));

  for (i = 0; i < ncodes; i++)
    if (ascii_strcasecmp (tocodes[i], "utf-8"))
      cd[i] = mutt_iconv_open (tocodes[i], "utf-8", 0);
    else
      /* Special case for conversion to UTF-8 */
      cd[i] = (iconv_t)(-1), score[i] = (size_t)(-1);

  rewind (file);
  ibl = 0;
  for (;;)
  {

    /* Try to fill input buffer */
    n = fread (bufi + ibl, 1, sizeof (bufi) - ibl, file);
    ibl += n;

    /* Convert to UTF-8 */
    ib = bufi;
    ob = bufu, obl = sizeof (bufu);
    n = iconv (cd1, ibl ? &ib : 0, &ibl, &ob, &obl);
    assert (n == (size_t)(-1) || !n || ICONV_NONTRANS);
    if (n == (size_t)(-1) &&
	((errno != EINVAL && errno != E2BIG) || ib == bufi))
    {
      assert (errno == EILSEQ ||
	      (errno == EINVAL && ib == bufi && ibl < sizeof (bufi)));
      ret = (size_t)(-1);
      break;
    }
    ubl1 = ob - bufu;

    /* Convert from UTF-8 */
    for (i = 0; i < ncodes; i++)
      if (cd[i] != (iconv_t)(-1) && score[i] != (size_t)(-1))
      {
	ub = bufu, ubl = ubl1;
	ob = bufo, obl = sizeof (bufo);
	n = iconv (cd[i], (ibl || ubl) ? &ub : 0, &ubl, &ob, &obl);
	if (n == (size_t)(-1))
	{
	  assert (errno == E2BIG ||
		  (BUGGY_ICONV && (errno == EILSEQ || errno == ENOENT)));
	  score[i] = (size_t)(-1);
	}
	else
	{
	  score[i] += n;
	  update_content_info (&infos[i], &states[i], bufo, ob - bufo);
	}
      }
      else if (cd[i] == (iconv_t)(-1) && score[i] == (size_t)(-1))
	/* Special case for conversion to UTF-8 */
	update_content_info (&infos[i], &states[i], bufu, ubl1);

    if (ibl)
      /* Save unused input */
      memmove (bufi, ib, ibl);
    else if (!ubl1 && ib < bufi + sizeof (bufi))
    {
      ret = 0;
      break;
    }
  }

  if (!ret)
  {
    /* Find best score */
    ret = (size_t)(-1);
    for (i = 0; i < ncodes; i++)
    {
      if (cd[i] == (iconv_t)(-1) && score[i] == (size_t)(-1))
      {
	/* Special case for conversion to UTF-8 */
	*tocode = i;
	ret = 0;
	break;
      }
      else if (cd[i] == (iconv_t)(-1) || score[i] == (size_t)(-1))
	continue;
      else if (ret == (size_t)(-1) || score[i] < ret)
      {
	*tocode = i;
	ret = score[i];
	if (!ret)
	  break;
      }
    }
    if (ret != (size_t)(-1))
    {
      memcpy (info, &infos[*tocode], sizeof(CONTENT));
      update_content_info (info, &states[*tocode], 0, 0); /* EOF */
    }
  }

  for (i = 0; i < ncodes; i++)
    if (cd[i] != (iconv_t)(-1))
      iconv_close (cd[i]);

  iconv_close (cd1);
  FREE (&cd);
  FREE (&infos);
  FREE (&score);
  FREE (&states);

  return ret;
#else
  return -1;
#endif /* !HAVE_ICONV */
}

/*
 * Find the first of the fromcodes that gives a valid conversion and
 * the best charset conversion of the file into one of the tocodes. If
 * successful, set *fromcode and *tocode to dynamically allocated
 * strings, set CONTENT *info, and return the number of characters
 * converted inexactly. If no conversion was possible, return -1.
 *
 * Both fromcodes and tocodes may be colon-separated lists of charsets.
 * However, if fromcode is zero then fromcodes is assumed to be the
 * name of a single charset even if it contains a colon.
 */
static size_t convert_file_from_to (FILE *file,
				    const char *fromcodes, const char *tocodes,
				    char **fromcode, char **tocode, CONTENT *info)
{
  char *fcode = NULL;
  char **tcode;
  const char *c, *c1;
  size_t ret;
  int ncodes, i, cn;

  /* Count the tocodes */
  ncodes = 0;
  for (c = tocodes; c; c = c1 ? c1 + 1 : 0)
  {
    if ((c1 = strchr (c, ':')) == c)
      continue;
    ++ncodes;
  }

  /* Copy them */
  tcode = safe_malloc (ncodes * sizeof (char *));
  for (c = tocodes, i = 0; c; c = c1 ? c1 + 1 : 0, i++)
  {
    if ((c1 = strchr (c, ':')) == c)
      continue;
    tcode[i] = mutt_substrdup (c, c1);
  }

  ret = (size_t)(-1);
  if (fromcode)
  {
    /* Try each fromcode in turn */
    for (c = fromcodes; c; c = c1 ? c1 + 1 : 0)
    {
      if ((c1 = strchr (c, ':')) == c)
	continue;
      fcode = mutt_substrdup (c, c1);

      ret = convert_file_to (file, fcode, ncodes, (const char **)tcode,
			     &cn, info);
      if (ret != (size_t)(-1))
      {
	*fromcode = fcode;
	*tocode = tcode[cn];
	tcode[cn] = 0;
	break;
      }
      FREE (&fcode);
    }
  }
  else
  {
    /* There is only one fromcode */
    ret = convert_file_to (file, fromcodes, ncodes, (const char **)tcode,
			   &cn, info);
    if (ret != (size_t)(-1))
    {
      *tocode = tcode[cn];
      tcode[cn] = 0;
    }
  }

  /* Free memory */
  for (i = 0; i < ncodes; i++)
    FREE (&tcode[i]);

  FREE (&tcode);

  return ret;
}

/*
 * Analyze the contents of a file to determine which MIME encoding to use.
 * Also set the body charset, sometimes, or not.
 */
CONTENT *mutt_get_content_info (const char *fname, BODY *b)
{
  CONTENT *info;
  CONTENT_STATE state;
  FILE *fp = NULL;
  char *fromcode = NULL;
  char *tocode;
  char buffer[100];
  char chsbuf[STRING];
  size_t r;

  struct stat sb;

  if(b && !fname) fname = b->filename;

  if (stat (fname, &sb) == -1)
  {
    mutt_error (_("Can't stat %s: %s"), fname, strerror (errno));
    return NULL;
  }

  if (!S_ISREG(sb.st_mode))
  {
    mutt_error (_("%s isn't a regular file."), fname);
    return NULL;
  }

  if ((fp = fopen (fname, "r")) == NULL)
  {
    dprint (1, (debugfile, "mutt_get_content_info: %s: %s (errno %d).\n",
		fname, strerror (errno), errno));
    return (NULL);
  }

  info = safe_calloc (1, sizeof (CONTENT));
  memset (&state, 0, sizeof (state));

  if (b != NULL && b->type == TYPETEXT && (!b->noconv && !b->force_charset))
  {
    char *chs = mutt_get_parameter ("charset", b->parameter);
    char *fchs = b->use_disp ? ((AttachCharset && *AttachCharset) ?
                                AttachCharset : Charset) : Charset;
    if (Charset && (chs || SendCharset) &&
        convert_file_from_to (fp, fchs, chs ? chs : SendCharset,
                              &fromcode, &tocode, info) != (size_t)(-1))
    {
      if (!chs)
      {
	mutt_canonical_charset (chsbuf, sizeof (chsbuf), tocode);
	mutt_set_parameter ("charset", chsbuf, &b->parameter);
      }
      FREE (&b->charset);
      b->charset = fromcode;
      FREE (&tocode);
      safe_fclose (&fp);
      return info;
    }
  }

  rewind (fp);
  while ((r = fread (buffer, 1, sizeof(buffer), fp)))
    update_content_info (info, &state, buffer, r);
  update_content_info (info, &state, 0, 0);

  safe_fclose (&fp);

  if (b != NULL && b->type == TYPETEXT && (!b->noconv && !b->force_charset))
    mutt_set_parameter ("charset", (!info->hibin ? "us-ascii" :
				    Charset  && !mutt_is_us_ascii (Charset) ? Charset : "unknown-8bit"),
			&b->parameter);

  return info;
}

/* Given a file with path ``s'', see if there is a registered MIME type.
 * returns the major MIME type, and copies the subtype to ``d''.  First look
 * for ~/.mime.types, then look in a system mime.types if we can find one.
 * The longest match is used so that we can match `ps.gz' when `gz' also
 * exists.
 */

int mutt_lookup_mime_type (BODY *att, const char *path)
{
  FILE *f;
  char *p, *q, *ct;
  char buf[LONG_STRING];
  char subtype[STRING], xtype[STRING];
  int count;
  int szf, sze, cur_sze;
  int type;

  *subtype = '\0';
  *xtype   = '\0';
  type     = TYPEOTHER;
  cur_sze  = 0;

  szf      = mutt_strlen (path);

  for (count = 0 ; count < 3 ; count++)
  {
    /*
     * can't use strtok() because we use it in an inner loop below, so use
     * a switch statement here instead.
     */
    switch (count)
    {
      case 0:
	snprintf (buf, sizeof (buf), "%s/.mime.types", NONULL(Homedir));
	break;
      case 1:
	strfcpy (buf, SYSCONFDIR"/mime.types", sizeof(buf));
	break;
      case 2:
	strfcpy (buf, PKGDATADIR"/mime.types", sizeof (buf));
	break;
      default:
        dprint (1, (debugfile, "mutt_lookup_mime_type: Internal error, count = %d.\n", count));
	goto bye;	/* shouldn't happen */
    }

    if ((f = fopen (buf, "r")) != NULL)
    {
      while (fgets (buf, sizeof (buf) - 1, f) != NULL)
      {
	/* weed out any comments */
	if ((p = strchr (buf, '#')))
	  *p = 0;

	/* remove any leading space. */
	ct = buf;
	SKIPWS (ct);

	/* position on the next field in this line */
	if ((p = strpbrk (ct, " \t")) == NULL)
	  continue;
	*p++ = 0;
	SKIPWS (p);

	/* cycle through the file extensions */
	while ((p = strtok (p, " \t\n")))
	{
	  sze = mutt_strlen (p);
	  if ((sze > cur_sze) && (szf >= sze) &&
	      (mutt_strcasecmp (path + szf - sze, p) == 0 || ascii_strcasecmp (path + szf - sze, p) == 0) &&
	      (szf == sze || path[szf - sze - 1] == '.'))
	  {
	    /* get the content-type */

	    if ((p = strchr (ct, '/')) == NULL)
	    {
	      /* malformed line, just skip it. */
	      break;
	    }
	    *p++ = 0;

	    for (q = p; *q && !ISSPACE (*q); q++)
	      ;

	    mutt_substrcpy (subtype, p, q, sizeof (subtype));

	    if ((type = mutt_check_mime_type (ct)) == TYPEOTHER)
	      strfcpy (xtype, ct, sizeof (xtype));

	    cur_sze = sze;
	  }
	  p = NULL;
	}
      }
      safe_fclose (&f);
    }
  }

 bye:

  if (type != TYPEOTHER || *xtype != '\0')
  {
    att->type = type;
    mutt_str_replace (&att->subtype, subtype);
    mutt_str_replace (&att->xtype, xtype);
  }

  return (type);
}

void mutt_message_to_7bit (BODY *a, FILE *fp)
{
  char temp[_POSIX_PATH_MAX];
  char *line = NULL;
  FILE *fpin = NULL;
  FILE *fpout = NULL;
  struct stat sb;

  if (!a->filename && fp)
    fpin = fp;
  else if (!a->filename || !(fpin = fopen (a->filename, "r")))
  {
    mutt_error (_("Could not open %s"), a->filename ? a->filename : "(null)");
    return;
  }
  else
  {
    a->offset = 0;
    if (stat (a->filename, &sb) == -1)
    {
      mutt_perror ("stat");
      safe_fclose (&fpin);
    }
    a->length = sb.st_size;
  }

  mutt_mktemp (temp, sizeof (temp));
  if (!(fpout = safe_fopen (temp, "w+")))
  {
    mutt_perror ("fopen");
    goto cleanup;
  }

  fseeko (fpin, a->offset, 0);
  a->parts = mutt_parse_messageRFC822 (fpin, a);

  transform_to_7bit (a->parts, fpin);

  mutt_copy_hdr (fpin, fpout, a->offset, a->offset + a->length,
		 CH_MIME | CH_NONEWLINE | CH_XMIT, NULL);

  fputs ("MIME-Version: 1.0\n", fpout);
  mutt_write_mime_header (a->parts, fpout);
  fputc ('\n', fpout);
  mutt_write_mime_body (a->parts, fpout);

 cleanup:
  FREE (&line);

  if (fpin && fpin != fp)
    safe_fclose (&fpin);
  if (fpout)
    safe_fclose (&fpout);
  else
    return;

  a->encoding = ENC7BIT;
  FREE (&a->d_filename);
  a->d_filename = a->filename;
  if (a->filename && a->unlink)
    unlink (a->filename);
  a->filename = safe_strdup (temp);
  a->unlink = 1;
  if(stat (a->filename, &sb) == -1)
  {
    mutt_perror ("stat");
    return;
  }
  a->length = sb.st_size;
  mutt_free_body (&a->parts);
  a->hdr->content = NULL;
}

static void transform_to_7bit (BODY *a, FILE *fpin)
{
  char buff[_POSIX_PATH_MAX];
  STATE s;
  struct stat sb;

  memset (&s, 0, sizeof (s));
  for (; a; a = a->next)
  {
    if (a->type == TYPEMULTIPART)
    {
      if (a->encoding != ENC7BIT)
	a->encoding = ENC7BIT;

      transform_to_7bit (a->parts, fpin);
    }
    else if (mutt_is_message_type(a->type, a->subtype))
    {
      mutt_message_to_7bit (a, fpin);
    }
    else
    {
      a->noconv = 1;
      a->force_charset = 1;

      mutt_mktemp (buff, sizeof (buff));
      if ((s.fpout = safe_fopen (buff, "w")) == NULL)
      {
	mutt_perror ("fopen");
	return;
      }
      s.fpin = fpin;
      mutt_decode_attachment (a, &s);
      safe_fclose (&s.fpout);
      FREE (&a->d_filename);
      a->d_filename = a->filename;
      a->filename = safe_strdup (buff);
      a->unlink = 1;
      if (stat (a->filename, &sb) == -1)
      {
	mutt_perror ("stat");
	return;
      }
      a->length = sb.st_size;

      mutt_update_encoding (a);
      if (a->encoding == ENC8BIT)
	a->encoding = ENCQUOTEDPRINTABLE;
      else if(a->encoding == ENCBINARY)
	a->encoding = ENCBASE64;
    }
  }
}

/* determine which Content-Transfer-Encoding to use */
static void mutt_set_encoding (BODY *b, CONTENT *info)
{
  char send_charset[SHORT_STRING];

  if (b->type == TYPETEXT)
  {
    char *chsname = mutt_get_body_charset (send_charset, sizeof (send_charset), b);
    if ((info->lobin && ascii_strncasecmp (chsname, "iso-2022", 8)) || info->linemax > 990 || (info->from && option (OPTENCODEFROM)))
      b->encoding = ENCQUOTEDPRINTABLE;
    else if (info->hibin)
      b->encoding = option (OPTALLOW8BIT) ? ENC8BIT : ENCQUOTEDPRINTABLE;
    else
      b->encoding = ENC7BIT;
  }
  else if (b->type == TYPEMESSAGE || b->type == TYPEMULTIPART)
  {
    if (info->lobin || info->hibin)
    {
      if (option (OPTALLOW8BIT) && !info->lobin)
	b->encoding = ENC8BIT;
      else
	mutt_message_to_7bit (b, NULL);
    }
    else
      b->encoding = ENC7BIT;
  }
  else if (b->type == TYPEAPPLICATION && ascii_strcasecmp (b->subtype, "pgp-keys") == 0)
    b->encoding = ENC7BIT;
  else
#if 0
    if (info->lobin || info->hibin || info->binary || info->linemax > 990
	   || info->cr || (/* option (OPTENCODEFROM) && */ info->from))
#endif
  {
    /* Determine which encoding is smaller  */
    if (1.33 * (float)(info->lobin+info->hibin+info->ascii) <
	 3.0 * (float)(info->lobin + info->hibin) + (float)info->ascii)
      b->encoding = ENCBASE64;
    else
      b->encoding = ENCQUOTEDPRINTABLE;
  }
#if 0
  else
    b->encoding = ENC7BIT;
#endif
}

void mutt_stamp_attachment(BODY *a)
{
  a->stamp = time(NULL);
}

/* Get a body's character set */

char *mutt_get_body_charset (char *d, size_t dlen, BODY *b)
{
  char *p = NULL;

  if (b && b->type != TYPETEXT)
    return NULL;

  if (b)
    p = mutt_get_parameter ("charset", b->parameter);

  if (p)
    mutt_canonical_charset (d, dlen, NONULL(p));
  else
    strfcpy (d, "us-ascii", dlen);

  return d;
}


/* Assumes called from send mode where BODY->filename points to actual file */
void mutt_update_encoding (BODY *a)
{
  CONTENT *info;
  char chsbuff[STRING];

  /* override noconv when it's us-ascii */
  if (mutt_is_us_ascii (mutt_get_body_charset (chsbuff, sizeof (chsbuff), a)))
    a->noconv = 0;

  if (!a->force_charset && !a->noconv)
    mutt_delete_parameter ("charset", &a->parameter);

  if ((info = mutt_get_content_info (a->filename, a)) == NULL)
    return;

  mutt_set_encoding (a, info);
  mutt_stamp_attachment(a);

  FREE (&a->content);
  a->content = info;

}

BODY *mutt_make_message_attach (CONTEXT *ctx, HEADER *hdr, int attach_msg)
{
  char buffer[LONG_STRING];
  BODY *body;
  FILE *fp;
  int cmflags, chflags;
  int pgp = WithCrypto? hdr->security : 0;

  if (WithCrypto)
  {
    if ((option(OPTMIMEFORWDECODE) || option(OPTFORWDECRYPT)) &&
        (hdr->security & ENCRYPT)) {
      if (!crypt_valid_passphrase(hdr->security))
        return (NULL);
    }
  }

  mutt_mktemp (buffer, sizeof (buffer));
  if ((fp = safe_fopen (buffer, "w+")) == NULL)
    return NULL;

  body = mutt_new_body ();
  body->type = TYPEMESSAGE;
  body->subtype = safe_strdup ("rfc822");
  body->filename = safe_strdup (buffer);
  body->unlink = 1;
  body->use_disp = 0;
  body->disposition = DISPINLINE;
  body->noconv = 1;

  mutt_parse_mime_message (ctx, hdr);

  chflags = CH_XMIT;
  cmflags = 0;

  /* If we are attaching a message, ignore OPTMIMEFORWDECODE */
  if (!attach_msg && option (OPTMIMEFORWDECODE))
  {
    chflags |= CH_MIME | CH_TXTPLAIN;
    cmflags = MUTT_CM_DECODE | MUTT_CM_CHARCONV;
    if ((WithCrypto & APPLICATION_PGP))
      pgp &= ~PGPENCRYPT;
    if ((WithCrypto & APPLICATION_SMIME))
      pgp &= ~SMIMEENCRYPT;
  }
  else if (WithCrypto
           && option (OPTFORWDECRYPT) && (hdr->security & ENCRYPT))
  {
    if ((WithCrypto & APPLICATION_PGP)
        && mutt_is_multipart_encrypted (hdr->content))
    {
      chflags |= CH_MIME | CH_NONEWLINE;
      cmflags = MUTT_CM_DECODE_PGP;
      pgp &= ~PGPENCRYPT;
    }
    else if ((WithCrypto & APPLICATION_PGP)
             && (mutt_is_application_pgp (hdr->content) & PGPENCRYPT))
    {
      chflags |= CH_MIME | CH_TXTPLAIN;
      cmflags = MUTT_CM_DECODE | MUTT_CM_CHARCONV;
      pgp &= ~PGPENCRYPT;
    }
    else if ((WithCrypto & APPLICATION_SMIME)
              && mutt_is_application_smime (hdr->content) & SMIMEENCRYPT)
    {
      chflags |= CH_MIME | CH_TXTPLAIN;
      cmflags = MUTT_CM_DECODE | MUTT_CM_CHARCONV;
      pgp &= ~SMIMEENCRYPT;
    }
  }

  mutt_copy_message (fp, ctx, hdr, cmflags, chflags);

  fflush(fp);
  rewind(fp);

  body->hdr = mutt_new_header();
  body->hdr->offset = 0;
  /* we don't need the user headers here */
  body->hdr->env = mutt_read_rfc822_header(fp, body->hdr, 0, 0);
  if (WithCrypto)
    body->hdr->security = pgp;
  mutt_update_encoding (body);
  body->parts = body->hdr->content;

  safe_fclose (&fp);

  return (body);
}

BODY *mutt_make_file_attach (const char *path)
{
  BODY *att;
  CONTENT *info;

  att = mutt_new_body ();
  att->filename = safe_strdup (path);

  /* Attempt to determine the appropriate content-type based on the filename
   * suffix.
   */

#if 0

  if ((n = mutt_lookup_mime_type (buf, sizeof (buf), xbuf, sizeof (xbuf), path)) != TYPEOTHER
      || *xbuf != '\0')
  {
    att->type = n;
    att->subtype = safe_strdup (buf);
    att->xtype = safe_strdup (xbuf);
  }

#else

  mutt_lookup_mime_type (att, path);

#endif

  if ((info = mutt_get_content_info (path, att)) == NULL)
  {
    mutt_free_body (&att);
    return NULL;
  }

  if (!att->subtype)
  {
    if (info->lobin == 0 || (info->lobin + info->hibin + info->ascii)/ info->lobin >= 10)
    {
      /*
       * Statistically speaking, there should be more than 10% "lobin"
       * chars if this is really a binary file...
       */
      att->type = TYPETEXT;
      att->subtype = safe_strdup ("plain");
    }
    else
    {
      att->type = TYPEAPPLICATION;
      att->subtype = safe_strdup ("octet-stream");
    }
  }

  FREE(&info);
  mutt_update_encoding (att);
  return (att);
}

static int get_toplevel_encoding (BODY *a)
{
  int e = ENC7BIT;

  for (; a; a = a->next)
  {
    if (a->encoding == ENCBINARY)
      return (ENCBINARY);
    else if (a->encoding == ENC8BIT)
      e = ENC8BIT;
  }

  return (e);
}

/* check for duplicate boundary. return 1 if duplicate */
static int mutt_check_boundary (const char* boundary, BODY *b)
{
  char* p;

  if (b->parts && mutt_check_boundary (boundary, b->parts))
    return 1;

  if (b->next && mutt_check_boundary (boundary, b->next))
    return 1;

  if ((p = mutt_get_parameter ("boundary", b->parameter))
      && !ascii_strcmp (p, boundary))
    return 1;
  return 0;
}

BODY *mutt_make_multipart (BODY *b)
{
  BODY *new;

  new = mutt_new_body ();
  new->type = TYPEMULTIPART;
  new->subtype = safe_strdup ("mixed");
  new->encoding = get_toplevel_encoding (b);
  do
  {
    mutt_generate_boundary (&new->parameter);
    if (mutt_check_boundary (mutt_get_parameter ("boundary", new->parameter),
			     b))
      mutt_delete_parameter ("boundary", &new->parameter);
  }
  while (!mutt_get_parameter ("boundary", new->parameter));
  new->use_disp = 0;
  new->disposition = DISPINLINE;
  new->parts = b;

  return new;
}

/* remove the multipart body if it exists */
BODY *mutt_remove_multipart (BODY *b)
{
  BODY *t;

  if (b->parts)
  {
    t = b;
    b = b->parts;
    t->parts = NULL;
    mutt_free_body (&t);
  }
  return b;
}

char *mutt_make_date (char *s, size_t len)
{
  time_t t = time (NULL);
  struct tm *l = localtime (&t);
  time_t tz = mutt_local_tz (t);

  tz /= 60;

  snprintf (s, len,  "Date: %s, %d %s %d %02d:%02d:%02d %+03d%02d\n",
	    Weekdays[l->tm_wday], l->tm_mday, Months[l->tm_mon],
	    l->tm_year + 1900, l->tm_hour, l->tm_min, l->tm_sec,
	    (int) tz / 60, (int) abs ((int) tz) % 60);
  return (s);
}

/* wrapper around mutt_write_address() so we can handle very large
   recipient lists without needing a huge temporary buffer in memory */
void mutt_write_address_list (ADDRESS *adr, FILE *fp, int linelen, int display)
{
  ADDRESS *tmp;
  char buf[LONG_STRING];
  int count = 0;
  int len;

  while (adr)
  {
    tmp = adr->next;
    adr->next = NULL;
    buf[0] = 0;
    rfc822_write_address (buf, sizeof (buf), adr, display);
    len = mutt_strlen (buf);
    if (count && linelen + len > 74)
    {
      fputs ("\n\t", fp);
      linelen = len + 8; /* tab is usually about 8 spaces... */
    }
    else
    {
      if (count && adr->mailbox)
      {
	fputc (' ', fp);
	linelen++;
      }
      linelen += len;
    }
    fputs (buf, fp);
    adr->next = tmp;
    if (!adr->group && adr->next && adr->next->mailbox)
    {
      linelen++;
      fputc (',', fp);
    }
    adr = adr->next;
    count++;
  }
  fputc ('\n', fp);
}

/* arbitrary number of elements to grow the array by */
#define REF_INC 16

/* need to write the list in reverse because they are stored in reverse order
 * when parsed to speed up threading
 */
void mutt_write_references (LIST *r, FILE *f, int trim)
{
  LIST **ref = NULL;
  int refcnt = 0, refmax = 0;

  for ( ; (trim == 0 || refcnt < trim) && r ; r = r->next)
  {
    if (refcnt == refmax)
      safe_realloc (&ref, (refmax += REF_INC) * sizeof (LIST *));
    ref[refcnt++] = r;
  }

  while (refcnt-- > 0)
  {
    fputc (' ', f);
    fputs (ref[refcnt]->data, f);
    if (refcnt >= 1)
      fputc ('\n', f);
  }

  FREE (&ref);
}

static const char *find_word (const char *src)
{
  const char *p = src;

  while (p && *p && strchr (" \t\n", *p))
    p++;
  while (p && *p && !strchr (" \t\n", *p))
    p++;
  return p;
}

/* like wcwidth(), but gets const char* not wchar_t* */
static int my_width (const char *str, int col, int flags)
{
  wchar_t wc;
  int l, w = 0, nl = 0;
  const char *p = str;

  while (p && *p)
  {
    if (mbtowc (&wc, p, MB_CUR_MAX) >= 0)
    {
      l = wcwidth (wc);
      if (l < 0)
	l = 1;
      /* correctly calc tab stop, even for sending as the
       * line should look pretty on the receiving end */
      if (wc == L'\t' || (nl && wc == L' '))
      {
	nl = 0;
	l = 8 - (col % 8);
      }
      /* track newlines for display-case: if we have a space
       * after a newline, assume 8 spaces as for display we
       * always tab-fold */
      else if ((flags & CH_DISPLAY) && wc == '\n')
	nl = 1;
    }
    else
      l = 1;
    w += l;
    p++;
  }
  return w;
}

static int print_val (FILE *fp, const char *pfx, const char *value,
		      int flags, size_t col)
{
  while (value && *value)
  {
    if (fputc (*value, fp) == EOF)
      return -1;
    /* corner-case: break words longer than 998 chars by force,
     * mandated by RfC5322 */
    if (!(flags & CH_DISPLAY) && ++col >= 998)
    {
      if (fputs ("\n ", fp) < 0)
	return -1;
      col = 1;
    }
    if (*value == '\n')
    {
      if (*(value + 1) && pfx && *pfx && fputs (pfx, fp) == EOF)
	return -1;
      /* for display, turn folding spaces into folding tabs */
      if ((flags & CH_DISPLAY) && (*(value + 1) == ' ' || *(value + 1) == '\t'))
      {
	value++;
	while (*value && (*value == ' ' || *value == '\t'))
	  value++;
	if (fputc ('\t', fp) == EOF)
	  return -1;
	continue;
      }
    }
    value++;
  }
  return 0;
}

static int fold_one_header (FILE *fp, const char *tag, const char *value,
			      const char *pfx, int wraplen, int flags)
{
  const char *p = value, *next, *sp;
  char buf[HUGE_STRING] = "";
  int first = 1, enc, col = 0, w, l = 0, fold;

  dprint(4,(debugfile,"mwoh: pfx=[%s], tag=[%s], flags=%d value=[%s]\n",
	    pfx, tag, flags, value));

  if (tag && *tag && fprintf (fp, "%s%s: ", NONULL (pfx), tag) < 0)
    return -1;
  col = mutt_strlen (tag) + (tag && *tag ? 2 : 0) + mutt_strlen (pfx);

  while (p && *p)
  {
    fold = 0;

    /* find the next word and place it in `buf'. it may start with
     * whitespace we can fold before */
    next = find_word (p);
    l = MIN(sizeof (buf) - 1, next - p);
    memcpy (buf, p, l);
    buf[l] = 0;

    /* determine width: character cells for display, bytes for sending
     * (we get pure ascii only) */
    w = my_width (buf, col, flags);
    enc = mutt_strncmp (buf, "=?", 2) == 0;

    dprint(5,(debugfile,"mwoh: word=[%s], col=%d, w=%d, next=[0x0%x]\n",
	      buf, col, w, *next));

    /* insert a folding \n before the current word's lwsp except for
     * header name, first word on a line (word longer than wrap width)
     * and encoded words */
    if (!first && !enc && col && col + w >= wraplen)
    {
      col = mutt_strlen (pfx);
      fold = 1;
      if (fprintf (fp, "\n%s", NONULL(pfx)) <= 0)
	return -1;
    }

    /* print the actual word; for display, ignore leading ws for word
     * and fold with tab for readability */
    if ((flags & CH_DISPLAY) && fold)
    {
      char *p = buf;
      while (*p && (*p == ' ' || *p == '\t'))
      {
	p++;
	col--;
      }
      if (fputc ('\t', fp) == EOF)
	return -1;
      if (print_val (fp, pfx, p, flags, col) < 0)
	return -1;
      col += 8;
    }
    else if (print_val (fp, pfx, buf, flags, col) < 0)
      return -1;
    col += w;

    /* if the current word ends in \n, ignore all its trailing spaces
     * and reset column; this prevents us from putting only spaces (or
     * even none) on a line if the trailing spaces are located at our
     * current line width
     * XXX this covers ASCII space only, for display we probably
     * XXX want something like iswspace() here */
    sp = next;
    while (*sp && (*sp == ' ' || *sp == '\t'))
      sp++;
    if (*sp == '\n')
    {
      next = sp;
      col = 0;
    }

    p = next;
    first = 0;
  }

  /* if we have printed something but didn't \n-terminate it, do it
   * except the last word we printed ended in \n already */
  if (col && (l == 0 || buf[l - 1] != '\n'))
    if (putc ('\n', fp) == EOF)
      return -1;

  return 0;
}

static char *unfold_header (char *s)
{
  char *p = s, *q = s;

  while (p && *p)
  {
    /* remove CRLF prior to FWSP, turn \t into ' ' */
    if (*p == '\r' && *(p + 1) && *(p + 1) == '\n' && *(p + 2) &&
	(*(p + 2) == ' ' || *(p + 2) == '\t'))
    {
      *q++ = ' ';
      p += 3;
      continue;
    }
    /* remove LF prior to FWSP, turn \t into ' ' */
    else if (*p == '\n' && *(p + 1) && (*(p + 1) == ' ' || *(p + 1) == '\t'))
    {
      *q++ = ' ';
      p += 2;
      continue;
    }
    *q++ = *p++;
  }
  if (q)
    *q = 0;

  return s;
}

static int write_one_header (FILE *fp, int pfxw, int max, int wraplen,
			     const char *pfx, const char *start, const char *end,
			     int flags)
{
  char *tagbuf, *valbuf, *t;
  int is_from = ((end - start) > 5 &&
		 ascii_strncasecmp (start, "from ", 5) == 0);

  /* only pass through folding machinery if necessary for sending,
     never wrap From_ headers on sending */
  if (!(flags & CH_DISPLAY) && (pfxw + max <= wraplen || is_from))
  {
    valbuf = mutt_substrdup (start, end);
    dprint(4,(debugfile,"mwoh: buf[%s%s] short enough, "
	      "max width = %d <= %d\n",
	      NONULL(pfx), valbuf, max, wraplen));
    if (pfx && *pfx)
      if (fputs (pfx, fp) == EOF)
	return -1;
    if (!(t = strchr (valbuf, ':')))
    {
      dprint (1, (debugfile, "mwoh: warning: header not in "
		  "'key: value' format!\n"));
      return 0;
    }
    if (print_val (fp, pfx, valbuf, flags, mutt_strlen (pfx)) < 0)
    {
      FREE(&valbuf);
      return -1;
    }
    FREE(&valbuf);
  }
  else
  {
    t = strchr (start, ':');
    if (!t || t > end)
    {
      dprint (1, (debugfile, "mwoh: warning: header not in "
		  "'key: value' format!\n"));
      return 0;
    }
    if (is_from)
    {
      tagbuf = NULL;
      valbuf = mutt_substrdup (start, end);
    }
    else
    {
      tagbuf = mutt_substrdup (start, t);
      /* skip over the colon separating the header field name and value */
      ++t;

      /* skip over any leading whitespace (WSP, as defined in RFC5322)
       * NOTE: skip_email_wsp() does the wrong thing here.
       *       See tickets 3609 and 3716. */
      while (*t == ' ' || *t == '\t')
        t++;

      valbuf = mutt_substrdup (t, end);
    }
    dprint(4,(debugfile,"mwoh: buf[%s%s] too long, "
	      "max width = %d > %d\n",
	      NONULL(pfx), valbuf, max, wraplen));
    if (fold_one_header (fp, tagbuf, valbuf, pfx, wraplen, flags) < 0)
      return -1;
    FREE (&tagbuf);
    FREE (&valbuf);
  }
  return 0;
}

/* split several headers into individual ones and call write_one_header
 * for each one */
int mutt_write_one_header (FILE *fp, const char *tag, const char *value,
			   const char *pfx, int wraplen, int flags)
{
  char *p = (char *)value, *last, *line;
  int max = 0, w, rc = -1;
  int pfxw = mutt_strwidth (pfx);
  char *v = safe_strdup (value);

  if (!(flags & CH_DISPLAY) || option (OPTWEED))
    v = unfold_header (v);

  /* when not displaying, use sane wrap value */
  if (!(flags & CH_DISPLAY))
  {
    if (WrapHeaders < 78 || WrapHeaders > 998)
      wraplen = 78;
    else
      wraplen = WrapHeaders;
  }
  else if (wraplen <= 0 || wraplen > MuttIndexWindow->cols)
    wraplen = MuttIndexWindow->cols;

  if (tag)
  {
    /* if header is short enough, simply print it */
    if (!(flags & CH_DISPLAY) && mutt_strwidth (tag) + 2 + pfxw +
	mutt_strwidth (v) <= wraplen)
    {
      dprint(4,(debugfile,"mwoh: buf[%s%s: %s] is short enough\n",
		NONULL(pfx), tag, v));
      if (fprintf (fp, "%s%s: %s\n", NONULL(pfx), tag, v) <= 0)
	goto out;
      rc = 0;
      goto out;
    }
    else
    {
      rc = fold_one_header (fp, tag, v, pfx, wraplen, flags);
      goto out;
    }
  }

  p = last = line = (char *)v;
  while (p && *p)
  {
    p = strchr (p, '\n');

    /* find maximum line width in current header */
    if (p)
      *p = 0;
    if ((w = my_width (line, 0, flags)) > max)
      max = w;
    if (p)
      *p = '\n';

    if (!p)
      break;

    line = ++p;
    if (*p != ' ' && *p != '\t')
    {
      if (write_one_header (fp, pfxw, max, wraplen, pfx, last, p, flags) < 0)
	goto out;
      last = p;
      max = 0;
    }
  }

  if (last && *last)
    if (write_one_header (fp, pfxw, max, wraplen, pfx, last, p, flags) < 0)
      goto out;

  rc = 0;

out:
  FREE (&v);
  return rc;
}


/* Note: all RFC2047 encoding should be done outside of this routine, except
 * for the "real name."  This will allow this routine to be used more than
 * once, if necessary.
 *
 * Likewise, all IDN processing should happen outside of this routine.
 *
 * mode == 1  => "lite" mode (used for edit_hdrs)
 * mode == 0  => normal mode.  write full header + MIME headers
 * mode == -1 => write just the envelope info (used for postponing messages)
 *
 * privacy != 0 => will omit any headers which may identify the user.
 *               Output generated is suitable for being sent through
 * 		 anonymous remailer chains.
 *
 */



int mutt_write_rfc822_header (FILE *fp, ENVELOPE *env, BODY *attach,
			      int mode, int privacy)
{
  char buffer[LONG_STRING];
  char *p, *q;
  LIST *tmp = env->userhdrs;
  int has_agent = 0; /* user defined user-agent header field exists */

  if (mode == 0 && !privacy)
    fputs (mutt_make_date (buffer, sizeof(buffer)), fp);

  /* OPTUSEFROM is not consulted here so that we can still write a From:
   * field if the user sets it with the `my_hdr' command
   */
  if (env->from && !privacy)
  {
    buffer[0] = 0;
    rfc822_write_address (buffer, sizeof (buffer), env->from, 0);
    fprintf (fp, "From: %s\n", buffer);
  }

  if (env->sender && !privacy)
  {
    buffer[0] = 0;
    rfc822_write_address (buffer, sizeof (buffer), env->sender, 0);
    fprintf (fp, "Sender: %s\n", buffer);
  }

  if (env->to)
  {
    fputs ("To: ", fp);
    mutt_write_address_list (env->to, fp, 4, 0);
  }
  else if (mode > 0)
    fputs ("To: \n", fp);

  if (env->cc)
  {
    fputs ("Cc: ", fp);
    mutt_write_address_list (env->cc, fp, 4, 0);
  }
  else if (mode > 0)
    fputs ("Cc: \n", fp);

  if (env->bcc)
  {
    if(mode != 0 || option(OPTWRITEBCC))
    {
      fputs ("Bcc: ", fp);
      mutt_write_address_list (env->bcc, fp, 5, 0);
    }
  }
  else if (mode > 0)
    fputs ("Bcc: \n", fp);

  if (env->subject)
    mutt_write_one_header (fp, "Subject", env->subject, NULL, 0, 0);
  else if (mode == 1)
    fputs ("Subject: \n", fp);

  /* save message id if the user has set it */
  if (env->message_id && !privacy)
    fprintf (fp, "Message-ID: %s\n", env->message_id);

  if (env->reply_to)
  {
    fputs ("Reply-To: ", fp);
    mutt_write_address_list (env->reply_to, fp, 10, 0);
  }
  else if (mode > 0)
    fputs ("Reply-To: \n", fp);

  if (env->mail_followup_to)
  {
    fputs ("Mail-Followup-To: ", fp);
    mutt_write_address_list (env->mail_followup_to, fp, 18, 0);
  }

  if (mode <= 0)
  {
    if (env->references)
    {
      fputs ("References:", fp);
      mutt_write_references (env->references, fp, 10);
      fputc('\n', fp);
    }

    /* Add the MIME headers */
    fputs ("MIME-Version: 1.0\n", fp);
    mutt_write_mime_header (attach, fp);
  }

  if (env->in_reply_to)
  {
    fputs ("In-Reply-To:", fp);
    mutt_write_references (env->in_reply_to, fp, 0);
    fputc ('\n', fp);
  }

  /* Add any user defined headers */
  for (; tmp; tmp = tmp->next)
  {
    if ((p = strchr (tmp->data, ':')))
    {
      q = p;

      *p = '\0';

      p = skip_email_wsp(p + 1);
      if (!*p)
      {
	*q = ':';
	continue;  /* don't emit empty fields. */
      }

      /* check to see if the user has overridden the user-agent field */
      if (!ascii_strncasecmp ("user-agent", tmp->data, 10))
      {
	has_agent = 1;
	if (privacy)
	{
	  *q = ':';
	  continue;
	}
      }

      mutt_write_one_header (fp, tmp->data, p, NULL, 0, 0);
      *q = ':';
    }
  }

  if (mode == 0 && !privacy && option (OPTXMAILER) && !has_agent)
  {
    /* Add a vanity header */
    fprintf (fp, "User-Agent: Mutt/%s (%s)\n", MUTT_VERSION, ReleaseDate);
  }

  return (ferror (fp) == 0 ? 0 : -1);
}

static void encode_headers (LIST *h)
{
  char *tmp;
  char *p;
  int i;

  for (; h; h = h->next)
  {
    if (!(p = strchr (h->data, ':')))
      continue;

    i = p - h->data;
    p = skip_email_wsp(p + 1);
    tmp = safe_strdup (p);

    if (!tmp)
      continue;

    rfc2047_encode_string (&tmp);
    safe_realloc (&h->data, mutt_strlen (h->data) + 2 + mutt_strlen (tmp) + 1);

    sprintf (h->data + i, ": %s", NONULL (tmp));  /* __SPRINTF_CHECKED__ */

    FREE (&tmp);
  }
}

const char *mutt_fqdn(short may_hide_host)
{
  char *p = NULL;

  if(Fqdn && Fqdn[0] != '@')
  {
    p = Fqdn;

    if(may_hide_host && option(OPTHIDDENHOST))
    {
      if((p = strchr(Fqdn, '.')))
	p++;

      /* sanity check: don't hide the host if
       * the fqdn is something like detebe.org.
       */

      if(!p || !strchr(p, '.'))
	p = Fqdn;
    }
  }

  return p;
}

char *mutt_gen_msgid (void)
{
  char buf[SHORT_STRING];
  time_t now;
  struct tm *tm;
  const char *fqdn;

  now = time (NULL);
  tm = gmtime (&now);
  if(!(fqdn = mutt_fqdn(0)))
    fqdn = NONULL(Hostname);

  snprintf (buf, sizeof (buf), "<%d%02d%02d%02d%02d%02d.G%c%u@%s>",
	    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
	    tm->tm_min, tm->tm_sec, MsgIdPfx, (unsigned int)getpid (), fqdn);
  MsgIdPfx = (MsgIdPfx == 'Z') ? 'A' : MsgIdPfx + 1;
  return (safe_strdup (buf));
}

static void alarm_handler (int sig)
{
  SigAlrm = 1;
}

/* invoke sendmail in a subshell
   path	(in)		path to program to execute
   args	(in)		arguments to pass to program
   msg (in)		temp file containing message to send
   tempfile (out)	if sendmail is put in the background, this points
   			to the temporary file containing the stdout of the
			child process. If it is NULL, stderr and stdout
                        are not redirected. */
static int
send_msg (const char *path, char **args, const char *msg, char **tempfile)
{
  sigset_t set;
  int fd, st;
  pid_t pid, ppid;

  mutt_block_signals_system ();

  sigemptyset (&set);
  /* we also don't want to be stopped right now */
  sigaddset (&set, SIGTSTP);
  sigprocmask (SIG_BLOCK, &set, NULL);

  if (SendmailWait >= 0 && tempfile)
  {
    char tmp[_POSIX_PATH_MAX];

    mutt_mktemp (tmp, sizeof (tmp));
    *tempfile = safe_strdup (tmp);
  }

  if ((pid = fork ()) == 0)
  {
    struct sigaction act, oldalrm;

    /* save parent's ID before setsid() */
    ppid = getppid ();

    /* we want the delivery to continue even after the main process dies,
     * so we put ourselves into another session right away
     */
    setsid ();

    /* next we close all open files */
    close (0);
#if defined(OPEN_MAX)
    for (fd = tempfile ? 1 : 3; fd < OPEN_MAX; fd++)
      close (fd);
#elif defined(_POSIX_OPEN_MAX)
    for (fd = tempfile ? 1 : 3; fd < _POSIX_OPEN_MAX; fd++)
      close (fd);
#else
    if (tempfile)
    {
      close (1);
      close (2);
    }
#endif

    /* now the second fork() */
    if ((pid = fork ()) == 0)
    {
      /* "msg" will be opened as stdin */
      if (open (msg, O_RDONLY, 0) < 0)
      {
	unlink (msg);
	_exit (S_ERR);
      }
      unlink (msg);

      if (SendmailWait >= 0 && tempfile && *tempfile)
      {
	/* *tempfile will be opened as stdout */
	if (open (*tempfile, O_WRONLY | O_APPEND | O_CREAT | O_EXCL, 0600) < 0)
	  _exit (S_ERR);
	/* redirect stderr to *tempfile too */
	if (dup (1) < 0)
	  _exit (S_ERR);
      }
      else if (tempfile)
      {
	if (open ("/dev/null", O_WRONLY | O_APPEND) < 0)	/* stdout */
	  _exit (S_ERR);
	if (open ("/dev/null", O_RDWR | O_APPEND) < 0)		/* stderr */
	  _exit (S_ERR);
      }

      execvp (path, args);
      _exit (S_ERR);
    }
    else if (pid == -1)
    {
      unlink (msg);
      if (tempfile)
	FREE (tempfile);		/* __FREE_CHECKED__ */
      _exit (S_ERR);
    }

    /* SendmailWait > 0: interrupt waitpid() after SendmailWait seconds
     * SendmailWait = 0: wait forever
     * SendmailWait < 0: don't wait
     */
    if (SendmailWait > 0)
    {
      SigAlrm = 0;
      act.sa_handler = alarm_handler;
#ifdef SA_INTERRUPT
      /* need to make sure waitpid() is interrupted on SIGALRM */
      act.sa_flags = SA_INTERRUPT;
#else
      act.sa_flags = 0;
#endif
      sigemptyset (&act.sa_mask);
      sigaction (SIGALRM, &act, &oldalrm);
      alarm (SendmailWait);
    }
    else if (SendmailWait < 0)
      _exit (0xff & EX_OK);

    if (waitpid (pid, &st, 0) > 0)
    {
      st = WIFEXITED (st) ? WEXITSTATUS (st) : S_ERR;
      if (SendmailWait && st == (0xff & EX_OK) && tempfile && *tempfile)
      {
	unlink (*tempfile); /* no longer needed */
	FREE (tempfile);		/* __FREE_CHECKED__ */
      }
    }
    else
    {
      st = (SendmailWait > 0 && errno == EINTR && SigAlrm) ?
	      S_BKG : S_ERR;
      if (SendmailWait > 0 && tempfile && *tempfile)
      {
	unlink (*tempfile);
	FREE (tempfile);		/* __FREE_CHECKED__ */
      }
    }

    /* reset alarm; not really needed, but... */
    alarm (0);
    sigaction (SIGALRM, &oldalrm, NULL);

    if (kill (ppid, 0) == -1 && errno == ESRCH && tempfile && *tempfile)
    {
      /* the parent is already dead */
      unlink (*tempfile);
      FREE (tempfile);		/* __FREE_CHECKED__ */
    }

    _exit (st);
  }

  sigprocmask (SIG_UNBLOCK, &set, NULL);

  if (pid != -1 && waitpid (pid, &st, 0) > 0)
    st = WIFEXITED (st) ? WEXITSTATUS (st) : S_ERR; /* return child status */
  else
    st = S_ERR;	/* error */

  mutt_unblock_signals_system (1);

  return (st);
}

static char **
add_args (char **args, size_t *argslen, size_t *argsmax, ADDRESS *addr)
{
  for (; addr; addr = addr->next)
  {
    /* weed out group mailboxes, since those are for display only */
    if (addr->mailbox && !addr->group)
    {
      if (*argslen == *argsmax)
	safe_realloc (&args, (*argsmax += 5) * sizeof (char *));
      args[(*argslen)++] = addr->mailbox;
    }
  }
  return (args);
}

static char **
add_option (char **args, size_t *argslen, size_t *argsmax, char *s)
{
  if (*argslen == *argsmax)
    safe_realloc (&args, (*argsmax += 5) * sizeof (char *));
  args[(*argslen)++] = s;
  return (args);
}

int
mutt_invoke_sendmail (ADDRESS *from,	/* the sender */
		 ADDRESS *to, ADDRESS *cc, ADDRESS *bcc, /* recips */
		 const char *msg, /* file containing message */
		 int eightbit) /* message contains 8bit chars */
{
  char *ps = NULL, *path = NULL, *s = safe_strdup (Sendmail), *childout = NULL;
  char **args = NULL;
  size_t argslen = 0, argsmax = 0;
  int i;

  /* ensure that $sendmail is set to avoid a crash. http://dev.mutt.org/trac/ticket/3548 */
  if (!s)
  {
    mutt_error(_("$sendmail must be set in order to send mail."));
    return -1;
  }

  ps = s;
  i = 0;
  while ((ps = strtok (ps, " ")))
  {
    if (argslen == argsmax)
      safe_realloc (&args, sizeof (char *) * (argsmax += 5));

    if (i)
      args[argslen++] = ps;
    else
    {
      path = safe_strdup (ps);
      ps = strrchr (ps, '/');
      if (ps)
	ps++;
      else
	ps = path;
      args[argslen++] = ps;
    }
    ps = NULL;
    i++;
  }

  if (eightbit && option (OPTUSE8BITMIME))
    args = add_option (args, &argslen, &argsmax, "-B8BITMIME");

  if (option (OPTENVFROM))
  {
    if (EnvFrom)
    {
      args = add_option (args, &argslen, &argsmax, "-f");
      args = add_args   (args, &argslen, &argsmax, EnvFrom);
    }
    else if (from && !from->next)
    {
      args = add_option (args, &argslen, &argsmax, "-f");
      args = add_args   (args, &argslen, &argsmax, from);
    }
  }

  if (DsnNotify)
  {
    args = add_option (args, &argslen, &argsmax, "-N");
    args = add_option (args, &argslen, &argsmax, DsnNotify);
  }
  if (DsnReturn)
  {
    args = add_option (args, &argslen, &argsmax, "-R");
    args = add_option (args, &argslen, &argsmax, DsnReturn);
  }
  args = add_option (args, &argslen, &argsmax, "--");
  args = add_args (args, &argslen, &argsmax, to);
  args = add_args (args, &argslen, &argsmax, cc);
  args = add_args (args, &argslen, &argsmax, bcc);

  if (argslen == argsmax)
    safe_realloc (&args, sizeof (char *) * (++argsmax));

  args[argslen++] = NULL;

  if ((i = send_msg (path, args, msg, option(OPTNOCURSES) ? NULL : &childout)) != (EX_OK & 0xff))
  {
    if (i != S_BKG)
    {
      const char *e = mutt_strsysexit (i);

      e = mutt_strsysexit (i);
      mutt_error (_("Error sending message, child exited %d (%s)."), i, NONULL (e));
      if (childout)
      {
	struct stat st;

	if (stat (childout, &st) == 0 && st.st_size > 0)
	  mutt_do_pager (_("Output of the delivery process"), childout, 0, NULL);
      }
    }
  }
  else if (childout)
    unlink (childout);

  FREE (&childout);
  FREE (&path);
  FREE (&s);
  FREE (&args);

  if (i == (EX_OK & 0xff))
    i = 0;
  else if (i == S_BKG)
    i = 1;
  else
    i = -1;
  return (i);
}

/* For postponing (!final) do the necessary encodings only */
void mutt_prepare_envelope (ENVELOPE *env, int final)
{
  char buffer[LONG_STRING];

  if (final)
  {
    if (env->bcc && !(env->to || env->cc))
    {
      /* some MTA's will put an Apparently-To: header field showing the Bcc:
       * recipients if there is no To: or Cc: field, so attempt to suppress
       * it by using an empty To: field.
       */
      env->to = rfc822_new_address ();
      env->to->group = 1;
      env->to->next = rfc822_new_address ();

      buffer[0] = 0;
      rfc822_cat (buffer, sizeof (buffer), "undisclosed-recipients",
		  RFC822Specials);

      env->to->mailbox = safe_strdup (buffer);
    }

    mutt_set_followup_to (env);

    if (!env->message_id)
      env->message_id = mutt_gen_msgid ();
  }

  /* Take care of 8-bit => 7-bit conversion. */
  rfc2047_encode_adrlist (env->to, "To");
  rfc2047_encode_adrlist (env->cc, "Cc");
  rfc2047_encode_adrlist (env->bcc, "Bcc");
  rfc2047_encode_adrlist (env->from, "From");
  rfc2047_encode_adrlist (env->mail_followup_to, "Mail-Followup-To");
  rfc2047_encode_adrlist (env->reply_to, "Reply-To");
  rfc2047_encode_string (&env->x_label);

  if (env->subject)
  {
    rfc2047_encode_string (&env->subject);
  }
  encode_headers (env->userhdrs);
}

void mutt_unprepare_envelope (ENVELOPE *env)
{
  LIST *item;

  for (item = env->userhdrs; item; item = item->next)
    rfc2047_decode (&item->data);

  rfc822_free_address (&env->mail_followup_to);

  /* back conversions */
  rfc2047_decode_adrlist (env->to);
  rfc2047_decode_adrlist (env->cc);
  rfc2047_decode_adrlist (env->bcc);
  rfc2047_decode_adrlist (env->from);
  rfc2047_decode_adrlist (env->reply_to);
  rfc2047_decode (&env->subject);
  rfc2047_decode (&env->x_label);
}

static int _mutt_bounce_message (FILE *fp, HEADER *h, ADDRESS *to, const char *resent_from,
				  ADDRESS *env_from)
{
  int i, ret = 0;
  FILE *f;
  char date[SHORT_STRING], tempfile[_POSIX_PATH_MAX];
  MESSAGE *msg = NULL;

  if (!h)
  {
	  /* Try to bounce each message out, aborting if we get any failures. */
    for (i=0; i<Context->msgcount; i++)
      if (Context->hdrs[i]->tagged)
        ret |= _mutt_bounce_message (fp, Context->hdrs[i], to, resent_from, env_from);
    return ret;
  }

  /* If we failed to open a message, return with error */
  if (!fp && (msg = mx_open_message (Context, h->msgno)) == NULL)
    return -1;

  if (!fp) fp = msg->fp;

  mutt_mktemp (tempfile, sizeof (tempfile));
  if ((f = safe_fopen (tempfile, "w")) != NULL)
  {
    int ch_flags = CH_XMIT | CH_NONEWLINE | CH_NOQFROM;
    char* msgid_str;

    if (!option (OPTBOUNCEDELIVERED))
      ch_flags |= CH_WEED_DELIVERED;

    fseeko (fp, h->offset, 0);
    fprintf (f, "Resent-From: %s", resent_from);
    fprintf (f, "\nResent-%s", mutt_make_date (date, sizeof(date)));
    msgid_str = mutt_gen_msgid();
    fprintf (f, "Resent-Message-ID: %s\n", msgid_str);
    fputs ("Resent-To: ", f);
    mutt_write_address_list (to, f, 11, 0);
    mutt_copy_header (fp, h, f, ch_flags, NULL);
    fputc ('\n', f);
    mutt_copy_bytes (fp, f, h->content->length);
    safe_fclose (&f);
    FREE (&msgid_str);

#if USE_SMTP
    if (SmtpUrl)
      ret = mutt_smtp_send (env_from, to, NULL, NULL, tempfile,
                            h->content->encoding == ENC8BIT);
    else
#endif /* USE_SMTP */
    ret = mutt_invoke_sendmail (env_from, to, NULL, NULL, tempfile,
			  	h->content->encoding == ENC8BIT);
  }

  if (msg)
    mx_close_message (Context, &msg);

  return ret;
}

int mutt_bounce_message (FILE *fp, HEADER *h, ADDRESS *to)
{
  ADDRESS *from, *resent_to;
  const char *fqdn = mutt_fqdn (1);
  char resent_from[STRING];
  int ret;
  char *err;

  resent_from[0] = '\0';
  from = mutt_default_from ();

  /*
   * mutt_default_from() does not use $realname if the real name is not set
   * in $from, so we add it here.  The reason it is not added in
   * mutt_default_from() is that during normal sending, we execute
   * send-hooks and set the realname last so that it can be changed based
   * upon message criteria.
   */
  if (! from->personal)
    from->personal = safe_strdup(Realname);

  if (fqdn)
    rfc822_qualify (from, fqdn);

  rfc2047_encode_adrlist (from, "Resent-From");
  if (mutt_addrlist_to_intl (from, &err))
  {
    mutt_error (_("Bad IDN %s while preparing resent-from."),
		err);
    rfc822_free_address (&from);
    return -1;
  }
  rfc822_write_address (resent_from, sizeof (resent_from), from, 0);

  /*
   * prepare recipient list. idna conversion appears to happen before this
   * function is called, since the user receives confirmation of the address
   * list being bounced to.
   */
  resent_to = rfc822_cpy_adr(to, 0);
  rfc2047_encode_adrlist(resent_to, "Resent-To");

  ret = _mutt_bounce_message (fp, h, resent_to, resent_from, from);

  rfc822_free_address (&resent_to);
  rfc822_free_address (&from);

  return ret;
}


/* given a list of addresses, return a list of unique addresses */
ADDRESS *mutt_remove_duplicates (ADDRESS *addr)
{
  ADDRESS *top = addr;
  ADDRESS **last = &top;
  ADDRESS *tmp;
  int dup;

  while (addr)
  {
    for (tmp = top, dup = 0; tmp && tmp != addr; tmp = tmp->next)
    {
      if (tmp->mailbox && addr->mailbox &&
	  !ascii_strcasecmp (addr->mailbox, tmp->mailbox))
      {
	dup = 1;
	break;
      }
    }

    if (dup)
    {
      dprint (2, (debugfile, "mutt_remove_duplicates: Removing %s\n",
		  addr->mailbox));

      *last = addr->next;

      addr->next = NULL;
      rfc822_free_address(&addr);

      addr = *last;
    }
    else
    {
      last = &addr->next;
      addr = addr->next;
    }
  }

  return (top);
}

static void set_noconv_flags (BODY *b, short flag)
{
  for(; b; b = b->next)
  {
    if (b->type == TYPEMESSAGE || b->type == TYPEMULTIPART)
      set_noconv_flags (b->parts, flag);
    else if (b->type == TYPETEXT && b->noconv)
    {
      if (flag)
	mutt_set_parameter ("x-mutt-noconv", "yes", &b->parameter);
      else
	mutt_delete_parameter ("x-mutt-noconv", &b->parameter);
    }
  }
}

int mutt_write_fcc (const char *path, HEADER *hdr, const char *msgid, int post, char *fcc)
{
  CONTEXT f;
  MESSAGE *msg;
  char tempfile[_POSIX_PATH_MAX];
  FILE *tempfp = NULL;
  int r, need_buffy_cleanup = 0;
  struct stat st;
  char buf[SHORT_STRING];
  int onm_flags;

  if (post)
    set_noconv_flags (hdr->content, 1);

  if (mx_open_mailbox (path, MUTT_APPEND | MUTT_QUIET, &f) == NULL)
  {
    dprint (1, (debugfile, "mutt_write_fcc(): unable to open mailbox %s in append-mode, aborting.\n",
		path));
    return (-1);
  }

  /* We need to add a Content-Length field to avoid problems where a line in
   * the message body begins with "From "
   */
  if (f.magic == MUTT_MMDF || f.magic == MUTT_MBOX)
  {
    mutt_mktemp (tempfile, sizeof (tempfile));
    if ((tempfp = safe_fopen (tempfile, "w+")) == NULL)
    {
      mutt_perror (tempfile);
      mx_close_mailbox (&f, NULL);
      return (-1);
    }
    /* remember new mail status before appending message */
    need_buffy_cleanup = 1;
    stat (path, &st);
  }

  hdr->read = !post; /* make sure to put it in the `cur' directory (maildir) */
  onm_flags = MUTT_ADD_FROM;
  if (post)
    onm_flags |= MUTT_SET_DRAFT;
  if ((msg = mx_open_new_message (&f, hdr, onm_flags)) == NULL)
  {
    mx_close_mailbox (&f, NULL);
    return (-1);
  }

  /* post == 1 => postpone message. Set mode = -1 in mutt_write_rfc822_header()
   * post == 0 => Normal mode. Set mode = 0 in mutt_write_rfc822_header()
   * */
  mutt_write_rfc822_header (msg->fp, hdr->env, hdr->content, post ? -post : 0, 0);

  /* (postponment) if this was a reply of some sort, <msgid> contians the
   * Message-ID: of message replied to.  Save it using a special X-Mutt-
   * header so it can be picked up if the message is recalled at a later
   * point in time.  This will allow the message to be marked as replied if
   * the same mailbox is still open.
   */
  if (post && msgid)
    fprintf (msg->fp, "X-Mutt-References: %s\n", msgid);

  /* (postponment) save the Fcc: using a special X-Mutt- header so that
   * it can be picked up when the message is recalled
   */
  if (post && fcc)
    fprintf (msg->fp, "X-Mutt-Fcc: %s\n", fcc);

  if (f.magic == MUTT_MMDF || f.magic == MUTT_MBOX)
    fprintf (msg->fp, "Status: RO\n");

  /* mutt_write_rfc822_header() only writes out a Date: header with
   * mode == 0, i.e. _not_ postponment; so write out one ourself */
  if (post)
    fprintf (msg->fp, "%s", mutt_make_date (buf, sizeof (buf)));

  /* (postponment) if the mail is to be signed or encrypted, save this info */
  if ((WithCrypto & APPLICATION_PGP)
      && post && (hdr->security & APPLICATION_PGP))
  {
    fputs ("X-Mutt-PGP: ", msg->fp);
    if (hdr->security & ENCRYPT)
      fputc ('E', msg->fp);
    if (hdr->security & OPPENCRYPT)
      fputc ('O', msg->fp);
    if (hdr->security & SIGN)
    {
      fputc ('S', msg->fp);
      if (PgpSignAs && *PgpSignAs)
        fprintf (msg->fp, "<%s>", PgpSignAs);
    }
    if (hdr->security & INLINE)
      fputc ('I', msg->fp);
    fputc ('\n', msg->fp);
  }

  /* (postponment) if the mail is to be signed or encrypted, save this info */
  if ((WithCrypto & APPLICATION_SMIME)
      && post && (hdr->security & APPLICATION_SMIME))
  {
    fputs ("X-Mutt-SMIME: ", msg->fp);
    if (hdr->security & ENCRYPT) {
	fputc ('E', msg->fp);
	if (SmimeCryptAlg && *SmimeCryptAlg)
	    fprintf (msg->fp, "C<%s>", SmimeCryptAlg);
    }
    if (hdr->security & OPPENCRYPT)
      fputc ('O', msg->fp);
    if (hdr->security & SIGN) {
	fputc ('S', msg->fp);
	if (SmimeDefaultKey && *SmimeDefaultKey)
	    fprintf (msg->fp, "<%s>", SmimeDefaultKey);
    }
    if (hdr->security & INLINE)
      fputc ('I', msg->fp);
    fputc ('\n', msg->fp);
  }

#ifdef MIXMASTER
  /* (postponement) if the mail is to be sent through a mixmaster
   * chain, save that information
   */

  if (post && hdr->chain && hdr->chain)
  {
    LIST *p;

    fputs ("X-Mutt-Mix:", msg->fp);
    for (p = hdr->chain; p; p = p->next)
      fprintf (msg->fp, " %s", (char *) p->data);

    fputc ('\n', msg->fp);
  }
#endif

  if (tempfp)
  {
    char sasha[LONG_STRING];
    int lines = 0;

    mutt_write_mime_body (hdr->content, tempfp);

    /* make sure the last line ends with a newline.  Emacs doesn't ensure
     * this will happen, and it can cause problems parsing the mailbox
     * later.
     */
    fseek (tempfp, -1, 2);
    if (fgetc (tempfp) != '\n')
    {
      fseek (tempfp, 0, 2);
      fputc ('\n', tempfp);
    }

    fflush (tempfp);
    if (ferror (tempfp))
    {
      dprint (1, (debugfile, "mutt_write_fcc(): %s: write failed.\n", tempfile));
      safe_fclose (&tempfp);
      unlink (tempfile);
      mx_commit_message (msg, &f);	/* XXX - really? */
      mx_close_message (&f, &msg);
      mx_close_mailbox (&f, NULL);
      return -1;
    }

    /* count the number of lines */
    rewind (tempfp);
    while (fgets (sasha, sizeof (sasha), tempfp) != NULL)
      lines++;
    fprintf (msg->fp, "Content-Length: " OFF_T_FMT "\n", (LOFF_T) ftello (tempfp));
    fprintf (msg->fp, "Lines: %d\n\n", lines);

    /* copy the body and clean up */
    rewind (tempfp);
    r = mutt_copy_stream (tempfp, msg->fp);
    if (fclose (tempfp) != 0)
      r = -1;
    /* if there was an error, leave the temp version */
    if (!r)
      unlink (tempfile);
  }
  else
  {
    fputc ('\n', msg->fp); /* finish off the header */
    r = mutt_write_mime_body (hdr->content, msg->fp);
  }

  if (mx_commit_message (msg, &f) != 0)
    r = -1;
  mx_close_message (&f, &msg);
  mx_close_mailbox (&f, NULL);

  if (!post && need_buffy_cleanup)
    mutt_buffy_cleanup (path, &st);

  if (post)
    set_noconv_flags (hdr->content, 0);

  return r;
}
