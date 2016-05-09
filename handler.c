/*
 * Copyright (C) 1996-2000,2002,2010,2013 Michael R. Elkins <me@mutt.org>
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "mutt.h"
#include "mutt_curses.h"
#include "rfc1524.h"
#include "keymap.h"
#include "mime.h"
#include "copy.h"
#include "charset.h"
#include "mutt_crypt.h"
#include "rfc3676.h"

#define BUFI_SIZE 1000
#define BUFO_SIZE 2000


typedef int (*handler_t) (BODY *, STATE *);

const int Index_hex[128] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
     0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,
    -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};

const int Index_64[128] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-1,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};

static void state_prefix_put (const char *d, size_t dlen, STATE *s)
{
  if (s->prefix)
    while (dlen--)
      state_prefix_putc (*d++, s);
  else
    fwrite (d, dlen, 1, s->fpout);
}

static void mutt_convert_to_state(iconv_t cd, char *bufi, size_t *l, STATE *s)
{
  char bufo[BUFO_SIZE];
  ICONV_CONST char *ib;
  char *ob;
  size_t ibl, obl;

  if (!bufi)
  {
    if (cd != (iconv_t)(-1))
    {
      ob = bufo, obl = sizeof (bufo);
      iconv (cd, 0, 0, &ob, &obl);
      if (ob != bufo)
	state_prefix_put (bufo, ob - bufo, s);
    }
    return;
  }

  if (cd == (iconv_t)(-1))
  {
    state_prefix_put (bufi, *l, s);
    *l = 0;
    return;
  }

  ib = bufi, ibl = *l;
  for (;;)
  {
    ob = bufo, obl = sizeof (bufo);
    mutt_iconv (cd, &ib, &ibl, &ob, &obl, 0, "?");
    if (ob == bufo)
      break;
    state_prefix_put (bufo, ob - bufo, s);
  }
  memmove (bufi, ib, ibl);
  *l = ibl;
}

static void mutt_decode_xbit (STATE *s, long len, int istext, iconv_t cd)
{
  int c, ch;
  char bufi[BUFI_SIZE];
  size_t l = 0;

  if (istext)
  {
    state_set_prefix(s);

    while ((c = fgetc(s->fpin)) != EOF && len--)
    {
      if(c == '\r' && len)
      {
	if((ch = fgetc(s->fpin)) == '\n')
	{
	  c = ch;
	  len--;
	}
	else 
	  ungetc(ch, s->fpin);
      }

      bufi[l++] = c;
      if (l == sizeof (bufi))
	mutt_convert_to_state (cd, bufi, &l, s);
    }

    mutt_convert_to_state (cd, bufi, &l, s);
    mutt_convert_to_state (cd, 0, 0, s);

    state_reset_prefix (s);
  }
  else
    mutt_copy_bytes (s->fpin, s->fpout, len);
}

static int qp_decode_triple (char *s, char *d)
{
  /* soft line break */
  if (*s == '=' && !(*(s+1)))
    return 1;
  
  /* quoted-printable triple */
  if (*s == '=' &&
      isxdigit ((unsigned char) *(s+1)) &&
      isxdigit ((unsigned char) *(s+2)))
  {
    *d = (hexval (*(s+1)) << 4) | hexval (*(s+2));
    return 0;
  }
  
  /* something else */
  return -1;
}

static void qp_decode_line (char *dest, char *src, size_t *l,
			    int last)
{
  char *d, *s;
  char c = 0;

  int kind = -1;
  int soft = 0;

  /* decode the line */
  
  for (d = dest, s = src; *s;)
  {
    switch ((kind = qp_decode_triple (s, &c)))
    {
      case  0: *d++ = c; s += 3; break;	/* qp triple */
      case -1: *d++ = *s++;      break; /* single character */
      case  1: soft = 1; s++;	 break; /* soft line break */
    }
  }

  if (!soft && last == '\n')
  {
    /* neither \r nor \n as part of line-terminating CRLF
     * may be qp-encoded, so remove \r and \n-terminate;
     * see RfC2045, sect. 6.7, (1): General 8bit representation */
    if (kind == 0 && c == '\r')
      *(d-1) = '\n';
    else
      *d++ = '\n';
  }
  
  *d = '\0';
  *l = d - dest;
}

/* 
 * Decode an attachment encoded with quoted-printable.
 * 
 * Why doesn't this overflow any buffers?  First, it's guaranteed
 * that the length of a line grows when you _en_-code it to
 * quoted-printable.  That means that we always can store the
 * result in a buffer of at most the _same_ size.
 * 
 * Now, we don't special-case if the line we read with fgets()
 * isn't terminated.  We don't care about this, since STRING > 78,
 * so corrupted input will just be corrupted a bit more.  That
 * implies that STRING+1 bytes are always sufficient to store the
 * result of qp_decode_line.
 * 
 * Finally, at soft line breaks, some part of a multibyte character
 * may have been left over by mutt_convert_to_state().  This shouldn't
 * be more than 6 characters, so STRING + 7 should be sufficient
 * memory to store the decoded data.
 * 
 * Just to make sure that I didn't make some off-by-one error
 * above, we just use STRING*2 for the target buffer's size.
 * 
 */

static void mutt_decode_quoted (STATE *s, long len, int istext, iconv_t cd)
{
  char line[STRING];
  char decline[2*STRING];
  size_t l = 0;
  size_t linelen;      /* number of input bytes in `line' */
  size_t l3;
  
  int last;    /* store the last character in the input line */
  
  if (istext)
    state_set_prefix(s);

  while (len > 0)
  {
    last = 0;
    
    /*
     * It's ok to use a fixed size buffer for input, even if the line turns
     * out to be longer than this.  Just process the line in chunks.  This
     * really shouldn't happen according the MIME spec, since Q-P encoded
     * lines are at most 76 characters, but we should be liberal about what
     * we accept.
     */
    if (fgets (line, MIN ((ssize_t)sizeof (line), len + 1), s->fpin) == NULL)
      break;

    linelen = strlen(line);
    len -= linelen;

    /*
     * inspect the last character we read so we can tell if we got the
     * entire line.
     */
    last = linelen ? line[linelen - 1] : 0;

    /* chop trailing whitespace if we got the full line */
    if (last == '\n')
    {
      while (linelen > 0 && ISSPACE (line[linelen-1]))
       linelen--;
      line[linelen]=0;
    }

    /* decode and do character set conversion */
    qp_decode_line (decline + l, line, &l3, last);
    l += l3;
    mutt_convert_to_state (cd, decline, &l, s);
  }

  mutt_convert_to_state (cd, 0, 0, s);
  state_reset_prefix(s);
}

void mutt_decode_base64 (STATE *s, long len, int istext, iconv_t cd)
{
  char buf[5];
  int c1, c2, c3, c4, ch, cr = 0, i;
  char bufi[BUFI_SIZE];
  size_t l = 0;

  buf[4] = 0;

  if (istext) 
    state_set_prefix(s);

  while (len > 0)
  {
    for (i = 0 ; i < 4 && len > 0 ; len--)
    {
      if ((ch = fgetc (s->fpin)) == EOF)
	break;
      if (ch >= 0 && ch < 128 && (base64val(ch) != -1 || ch == '='))
	buf[i++] = ch;
    }
    if (i != 4)
    {
      /* "i" may be zero if there is trailing whitespace, which is not an error */
      if (i != 0)
	dprint (2, (debugfile, "%s:%d [mutt_decode_base64()]: "
	      "didn't get a multiple of 4 chars.\n", __FILE__, __LINE__));
      break;
    }

    c1 = base64val (buf[0]);
    c2 = base64val (buf[1]);
    ch = (c1 << 2) | (c2 >> 4);

    if (cr && ch != '\n') 
      bufi[l++] = '\r';

    cr = 0;
      
    if (istext && ch == '\r')
      cr = 1;
    else
      bufi[l++] = ch;

    if (buf[2] == '=')
      break;
    c3 = base64val (buf[2]);
    ch = ((c2 & 0xf) << 4) | (c3 >> 2);

    if (cr && ch != '\n')
      bufi[l++] = '\r';

    cr = 0;

    if (istext && ch == '\r')
      cr = 1;
    else
      bufi[l++] = ch;

    if (buf[3] == '=') break;
    c4 = base64val (buf[3]);
    ch = ((c3 & 0x3) << 6) | c4;

    if (cr && ch != '\n')
      bufi[l++] = '\r';
    cr = 0;

    if (istext && ch == '\r')
      cr = 1;
    else
      bufi[l++] = ch;
    
    if (l + 8 >= sizeof (bufi))
      mutt_convert_to_state (cd, bufi, &l, s);
  }

  if (cr) bufi[l++] = '\r';

  mutt_convert_to_state (cd, bufi, &l, s);
  mutt_convert_to_state (cd, 0, 0, s);

  state_reset_prefix(s);
}

static unsigned char decode_byte (char ch)
{
  if (ch == 96)
    return 0;
  return ch - 32;
}

static void mutt_decode_uuencoded (STATE *s, long len, int istext, iconv_t cd)
{
  char tmps[SHORT_STRING];
  char linelen, c, l, out;
  char *pt;
  char bufi[BUFI_SIZE];
  size_t k = 0;

  if(istext)
    state_set_prefix(s);
  
  while(len > 0)
  {
    if ((fgets(tmps, sizeof(tmps), s->fpin)) == NULL)
      return;
    len -= mutt_strlen(tmps);
    if ((!mutt_strncmp (tmps, "begin", 5)) && ISSPACE (tmps[5]))
      break;
  }
  while(len > 0)
  {
    if ((fgets(tmps, sizeof(tmps), s->fpin)) == NULL)
      return;
    len -= mutt_strlen(tmps);
    if (!mutt_strncmp (tmps, "end", 3))
      break;
    pt = tmps;
    linelen = decode_byte (*pt);
    pt++;
    for (c = 0; c < linelen;)
    {
      for (l = 2; l <= 6; l += 2)
      {
	out = decode_byte (*pt) << l;
	pt++;
	out |= (decode_byte (*pt) >> (6 - l));
	bufi[k++] = out;
	c++;
	if (c == linelen)
	  break;
      }
      mutt_convert_to_state (cd, bufi, &k, s);
      pt++;
    }
  }

  mutt_convert_to_state (cd, bufi, &k, s);
  mutt_convert_to_state (cd, 0, 0, s);
  
  state_reset_prefix(s);
}

/* ----------------------------------------------------------------------------
 * A (not so) minimal implementation of RFC1563.
 */

#define IndentSize (4)
    
enum { RICH_PARAM=0, RICH_BOLD, RICH_UNDERLINE, RICH_ITALIC, RICH_NOFILL, 
  RICH_INDENT, RICH_INDENT_RIGHT, RICH_EXCERPT, RICH_CENTER, RICH_FLUSHLEFT,
  RICH_FLUSHRIGHT, RICH_COLOR, RICH_LAST_TAG };

static const struct {
  const wchar_t *tag_name;
  int index;
} EnrichedTags[] = {
  { L"param",		RICH_PARAM },
  { L"bold",		RICH_BOLD },
  { L"italic",		RICH_ITALIC },
  { L"underline",	RICH_UNDERLINE },
  { L"nofill",		RICH_NOFILL },
  { L"excerpt",		RICH_EXCERPT },
  { L"indent",		RICH_INDENT },
  { L"indentright",	RICH_INDENT_RIGHT },
  { L"center",		RICH_CENTER },
  { L"flushleft",	RICH_FLUSHLEFT },
  { L"flushright",	RICH_FLUSHRIGHT },
  { L"flushboth",	RICH_FLUSHLEFT },
  { L"color",		RICH_COLOR },
  { L"x-color",		RICH_COLOR },
  { NULL,		-1 }
};

struct enriched_state
{
  wchar_t *buffer;
  wchar_t *line;
  wchar_t *param;
  size_t buff_len;
  size_t line_len;
  size_t line_used;
  size_t line_max;
  size_t indent_len;
  size_t word_len;
  size_t buff_used;
  size_t param_used;
  size_t param_len;
  int tag_level[RICH_LAST_TAG];
  int WrapMargin;
  STATE *s;
};

static void enriched_wrap (struct enriched_state *stte)
{
  int x;
  int extra;

  if (stte->line_len)
  {
    if (stte->tag_level[RICH_CENTER] || stte->tag_level[RICH_FLUSHRIGHT])
    {
      /* Strip trailing white space */
      size_t y = stte->line_used - 1;

      while (y && iswspace (stte->line[y]))
      {
	stte->line[y] = (wchar_t) '\0';
	y--;
	stte->line_used--;
	stte->line_len--;
      }
      if (stte->tag_level[RICH_CENTER])
      {
	/* Strip leading whitespace */
	y = 0;

	while (stte->line[y] && iswspace (stte->line[y]))
	  y++;
	if (y)
	{
	  size_t z;

	  for (z = y ; z <= stte->line_used; z++)
	  {
	    stte->line[z - y] = stte->line[z];
	  }

	  stte->line_len -= y;
	  stte->line_used -= y;
	}
      }
    }

    extra = stte->WrapMargin - stte->line_len - stte->indent_len -
      (stte->tag_level[RICH_INDENT_RIGHT] * IndentSize);
    if (extra > 0) 
    {
      if (stte->tag_level[RICH_CENTER]) 
      {
	x = extra / 2;
	while (x)
	{
	  state_putc (' ', stte->s);
	  x--;
	}
      } 
      else if (stte->tag_level[RICH_FLUSHRIGHT])
      {
	x = extra-1;
	while (x)
	{
	  state_putc (' ', stte->s);
	  x--;
	}
      }
    }
    state_putws ((const wchar_t*) stte->line, stte->s);
  }

  state_putc ('\n', stte->s);
  stte->line[0] = (wchar_t) '\0';
  stte->line_len = 0;
  stte->line_used = 0;
  stte->indent_len = 0;
  if (stte->s->prefix)
  {
    state_puts (stte->s->prefix, stte->s);
    stte->indent_len += mutt_strlen (stte->s->prefix);
  }

  if (stte->tag_level[RICH_EXCERPT])
  {
    x = stte->tag_level[RICH_EXCERPT];
    while (x) 
    {
      if (stte->s->prefix)
      {
	state_puts (stte->s->prefix, stte->s);
	    stte->indent_len += mutt_strlen (stte->s->prefix);
      }
      else
      {
	state_puts ("> ", stte->s);
	stte->indent_len += mutt_strlen ("> ");
      }
      x--;
    }
  }
  else
    stte->indent_len = 0;
  if (stte->tag_level[RICH_INDENT])
  {
    x = stte->tag_level[RICH_INDENT] * IndentSize;
    stte->indent_len += x;
    while (x) 
    {
      state_putc (' ', stte->s);
      x--;
    }
  }
}

static void enriched_flush (struct enriched_state *stte, int wrap)
{
  if (!stte->tag_level[RICH_NOFILL] && (stte->line_len + stte->word_len > 
      (stte->WrapMargin - (stte->tag_level[RICH_INDENT_RIGHT] * IndentSize) - 
       stte->indent_len)))
    enriched_wrap (stte);

  if (stte->buff_used)
  {
    stte->buffer[stte->buff_used] = (wchar_t) '\0';
    stte->line_used += stte->buff_used;
    if (stte->line_used > stte->line_max)
    {
      stte->line_max = stte->line_used;
      safe_realloc (&stte->line, (stte->line_max + 1) * sizeof (wchar_t));
    }
    wcscat (stte->line, stte->buffer);
    stte->line_len += stte->word_len;
    stte->word_len = 0;
    stte->buff_used = 0;
  }
  if (wrap)
    enriched_wrap(stte);
  fflush (stte->s->fpout);
}


static void enriched_putwc (wchar_t c, struct enriched_state *stte)
{
  if (stte->tag_level[RICH_PARAM]) 
  {
    if (stte->tag_level[RICH_COLOR]) 
    {
      if (stte->param_used + 1 >= stte->param_len)
	safe_realloc (&stte->param, (stte->param_len += STRING) * sizeof (wchar_t));

      stte->param[stte->param_used++] = c;
    }
    return; /* nothing to do */
  }

  /* see if more space is needed (plus extra for possible rich characters) */
  if (stte->buff_len < stte->buff_used + 3)
  {
    stte->buff_len += LONG_STRING;
    safe_realloc (&stte->buffer, (stte->buff_len + 1) * sizeof (wchar_t));
  }

  if ((!stte->tag_level[RICH_NOFILL] && iswspace (c)) || c == (wchar_t) '\0')
  {
    if (c == (wchar_t) '\t')
      stte->word_len += 8 - (stte->line_len + stte->word_len) % 8;
    else
      stte->word_len++;
    
    stte->buffer[stte->buff_used++] = c;
    enriched_flush (stte, 0);
  }
  else
  {
    if (stte->s->flags & MUTT_DISPLAY)
    {
      if (stte->tag_level[RICH_BOLD])
      {
	stte->buffer[stte->buff_used++] = c;
	stte->buffer[stte->buff_used++] = (wchar_t) '\010';
	stte->buffer[stte->buff_used++] = c;
      }
      else if (stte->tag_level[RICH_UNDERLINE])
      {

	stte->buffer[stte->buff_used++] = '_';
	stte->buffer[stte->buff_used++] = (wchar_t) '\010';
	stte->buffer[stte->buff_used++] = c;
      }
      else if (stte->tag_level[RICH_ITALIC])
      {
	stte->buffer[stte->buff_used++] = c;
	stte->buffer[stte->buff_used++] = (wchar_t) '\010';
	stte->buffer[stte->buff_used++] = '_';
      }
      else
      {
	stte->buffer[stte->buff_used++] = c;
      }
    }
    else
    {
      stte->buffer[stte->buff_used++] = c;
    }
    stte->word_len++;
  }
}

static void enriched_puts (const char *s, struct enriched_state *stte)
{
  const char *c;

  if (stte->buff_len < stte->buff_used + mutt_strlen (s))
  {
    stte->buff_len += LONG_STRING;
    safe_realloc (&stte->buffer, (stte->buff_len + 1) * sizeof (wchar_t));
  }
  c = s;
  while (*c)
  {
    stte->buffer[stte->buff_used++] = (wchar_t) *c;
    c++;
  }
}

static void enriched_set_flags (const wchar_t *tag, struct enriched_state *stte)
{
  const wchar_t *tagptr = tag;
  int i, j;

  if (*tagptr == (wchar_t) '/')
    tagptr++;
  
  for (i = 0, j = -1; EnrichedTags[i].tag_name; i++)
    if (wcscasecmp (EnrichedTags[i].tag_name, tagptr) == 0)
    {
      j = EnrichedTags[i].index;
      break;
    }

  if (j != -1)
  {
    if (j == RICH_CENTER || j == RICH_FLUSHLEFT || j == RICH_FLUSHRIGHT)
      enriched_flush (stte, 1);

    if (*tag == (wchar_t) '/')
    {
      if (stte->tag_level[j]) /* make sure not to go negative */
	stte->tag_level[j]--;
      if ((stte->s->flags & MUTT_DISPLAY) && j == RICH_PARAM && stte->tag_level[RICH_COLOR])
      {
	stte->param[stte->param_used] = (wchar_t) '\0';
	if (!wcscasecmp(L"black", stte->param))
	{
	  enriched_puts("\033[30m", stte);
	}
	else if (!wcscasecmp(L"red", stte->param))
	{
	  enriched_puts("\033[31m", stte);
	}
	else if (!wcscasecmp(L"green", stte->param))
	{
	  enriched_puts("\033[32m", stte);
	}
	else if (!wcscasecmp(L"yellow", stte->param))
	{
	  enriched_puts("\033[33m", stte);
	}
	else if (!wcscasecmp(L"blue", stte->param))
	{
	  enriched_puts("\033[34m", stte);
	}
	else if (!wcscasecmp(L"magenta", stte->param))
	{
	  enriched_puts("\033[35m", stte);
	}
	else if (!wcscasecmp(L"cyan", stte->param))
	{
	  enriched_puts("\033[36m", stte);
	}
	else if (!wcscasecmp(L"white", stte->param))
	{
	  enriched_puts("\033[37m", stte);
	}
      }
      if ((stte->s->flags & MUTT_DISPLAY) && j == RICH_COLOR)
      {
	enriched_puts("\033[0m", stte);
      }

      /* flush parameter buffer when closing the tag */
      if (j == RICH_PARAM)
      {
	stte->param_used = 0;
	stte->param[0] = (wchar_t) '\0';
      }
    }
    else
      stte->tag_level[j]++;

    if (j == RICH_EXCERPT)
      enriched_flush(stte, 1);
  }
}

static int text_enriched_handler (BODY *a, STATE *s)
{
  enum {
    TEXT, LANGLE, TAG, BOGUS_TAG, NEWLINE, ST_EOF, DONE
  } state = TEXT;

  long bytes = a->length;
  struct enriched_state stte;
  wchar_t wc = 0;
  int tag_len = 0;
  wchar_t tag[LONG_STRING + 1];

  memset (&stte, 0, sizeof (stte));
  stte.s = s;
  stte.WrapMargin = ((s->flags & MUTT_DISPLAY) ? (MuttIndexWindow->cols-4) :
                     ((MuttIndexWindow->cols-4)<72)?(MuttIndexWindow->cols-4):72);
  stte.line_max = stte.WrapMargin * 4;
  stte.line = (wchar_t *) safe_calloc (1, (stte.line_max + 1) * sizeof (wchar_t));
  stte.param = (wchar_t *) safe_calloc (1, (STRING) * sizeof (wchar_t));

  stte.param_len = STRING;
  stte.param_used = 0;

  if (s->prefix)
  {
    state_puts (s->prefix, s);
    stte.indent_len += mutt_strlen (s->prefix);
  }

  while (state != DONE)
  {
    if (state != ST_EOF)
    {
      if (!bytes || (wc = fgetwc (s->fpin)) == WEOF)
	state = ST_EOF;
      else
	bytes--;
    }

    switch (state)
    {
      case TEXT :
	switch (wc)
	{
	  case '<' :
	    state = LANGLE;
	    break;

	  case '\n' :
	    if (stte.tag_level[RICH_NOFILL])
	    {
	      enriched_flush (&stte, 1);
	    }
	    else 
	    {
	      enriched_putwc ((wchar_t) ' ', &stte);
	      state = NEWLINE;
	    }
	    break;

	  default:
	    enriched_putwc (wc, &stte);
	}
	break;

      case LANGLE :
	if (wc == (wchar_t) '<')
	{
	  enriched_putwc (wc, &stte);
	  state = TEXT;
	  break;
	}
	else
	{
	  tag_len = 0;
	  state = TAG;
	}
	/* Yes, fall through (it wasn't a <<, so this char is first in TAG) */
      case TAG :
	if (wc == (wchar_t) '>')
	{
	  tag[tag_len] = (wchar_t) '\0';
	  enriched_set_flags (tag, &stte);
	  state = TEXT;
	}
	else if (tag_len < LONG_STRING)  /* ignore overly long tags */
	  tag[tag_len++] = wc;
	else
	  state = BOGUS_TAG;
	break;

      case BOGUS_TAG :
	if (wc == (wchar_t) '>')
	  state = TEXT;
	break;

      case NEWLINE :
	if (wc == (wchar_t) '\n')
	  enriched_flush (&stte, 1);
	else
	{
	  ungetwc (wc, s->fpin);
	  bytes++;
	  state = TEXT;
	}
	break;

      case ST_EOF :
	enriched_putwc ((wchar_t) '\0', &stte);
        enriched_flush (&stte, 1);
	state = DONE;
	break;

      case DONE: /* not reached, but gcc complains if this is absent */
	break;
    }
  }

  state_putc ('\n', s); /* add a final newline */

  FREE (&(stte.buffer));
  FREE (&(stte.line));
  FREE (&(stte.param));

  return 0;
}                                                                              

/* for compatibility with metamail */
static int is_mmnoask (const char *buf)
{
  char tmp[LONG_STRING], *p, *q;
  int lng;

  if ((p = getenv ("MM_NOASK")) != NULL && *p)
  {
    if (mutt_strcmp (p, "1") == 0)
      return (1);

    strfcpy (tmp, p, sizeof (tmp));
    p = tmp;

    while ((p = strtok (p, ",")) != NULL)
    {
      if ((q = strrchr (p, '/')) != NULL)
      {
	if (*(q+1) == '*')
	{
	  if (ascii_strncasecmp (buf, p, q-p) == 0)
	    return (1);
	}
	else
	{
	  if (ascii_strcasecmp (buf, p) == 0)
	    return (1);
	}
      }
      else
      {
	lng = mutt_strlen (p);
	if (buf[lng] == '/' && mutt_strncasecmp (buf, p, lng) == 0)
	  return (1);
      }

      p = NULL;
    }
  }

  return (0);
}

/*
 * Returns:
 * 1    if the body part should be filtered by a mailcap entry prior to viewing inline.
 *
 * 0    otherwise
 */
static int mutt_is_autoview (BODY *b)
{
  char type[SHORT_STRING];
  int is_autoview = 0;

  snprintf (type, sizeof (type), "%s/%s", TYPE (b), b->subtype);

  if (option(OPTIMPLICITAUTOVIEW))
  {
    /* $implicit_autoview is essentially the same as "auto_view *" */
    is_autoview = 1;
  }
  else
  {
    /* determine if this type is on the user's auto_view list */
    LIST *t = AutoViewList;

    mutt_check_lookup_list (b, type, sizeof (type));
    for (; t; t = t->next) {
      int i = mutt_strlen (t->data) - 1;
      if ((i > 0 && t->data[i-1] == '/' && t->data[i] == '*' && 
            ascii_strncasecmp (type, t->data, i) == 0) ||
          ascii_strcasecmp (type, t->data) == 0)
        is_autoview = 1;
    }

    if (is_mmnoask (type))
      is_autoview = 1;
  }

  /* determine if there is a mailcap entry suitable for auto_view
   *
   * WARNING: type is altered by this call as a result of `mime_lookup' support */
  if (is_autoview)
    return rfc1524_mailcap_lookup(b, type, NULL, MUTT_AUTOVIEW);

  return 0;
}

#define TXTHTML     1
#define TXTPLAIN    2
#define TXTENRICHED 3

static int alternative_handler (BODY *a, STATE *s)
{
  BODY *choice = NULL;
  BODY *b;
  LIST *t;
  int type = 0;
  int mustfree = 0;
  int rc = 0;

  if (a->encoding == ENCBASE64 || a->encoding == ENCQUOTEDPRINTABLE ||
      a->encoding == ENCUUENCODED)
  {
    struct stat st;
    mustfree = 1;
    fstat (fileno (s->fpin), &st);
    b = mutt_new_body ();
    b->length = (long) st.st_size;
    b->parts = mutt_parse_multipart (s->fpin,
		  mutt_get_parameter ("boundary", a->parameter),
		  (long) st.st_size, ascii_strcasecmp ("digest", a->subtype) == 0);
  }
  else
    b = a;

  a = b;

  /* First, search list of preferred types */
  t = AlternativeOrderList;
  while (t && !choice)
  {
    char *c;
    int btlen;  /* length of basetype */
    int wild;	/* do we have a wildcard to match all subtypes? */

    c = strchr (t->data, '/');
    if (c)
    {
      wild = (c[1] == '*' && c[2] == 0);
      btlen = c - t->data;
    }
    else
    {
      wild = 1;
      btlen = mutt_strlen (t->data);
    }

    if (a && a->parts) 
      b = a->parts;
    else
      b = a;
    while (b)
    {
      const char *bt = TYPE(b);
      if (!ascii_strncasecmp (bt, t->data, btlen) && bt[btlen] == 0)
      {
	/* the basetype matches */
	if (wild || !ascii_strcasecmp (t->data + btlen + 1, b->subtype))
	{
	  choice = b;
	}
      }
      b = b->next;
    }
    t = t->next;
  }

  /* Next, look for an autoviewable type */
  if (!choice)
  {
    if (a && a->parts) 
      b = a->parts;
    else
      b = a;
    while (b)
    {
      if (mutt_is_autoview (b))
	choice = b;
      b = b->next;
    }
  }

  /* Then, look for a text entry */
  if (!choice)
  {
    if (a && a->parts) 
      b = a->parts;
    else
      b = a;
    while (b)
    {
      if (b->type == TYPETEXT)
      {
	if (! ascii_strcasecmp ("plain", b->subtype) && type <= TXTPLAIN)
	{
	  choice = b;
	  type = TXTPLAIN;
	}
	else if (! ascii_strcasecmp ("enriched", b->subtype) && type <= TXTENRICHED)
	{
	  choice = b;
	  type = TXTENRICHED;
	}
	else if (! ascii_strcasecmp ("html", b->subtype) && type <= TXTHTML)
	{
	  choice = b;
	  type = TXTHTML;
	}
      }
      b = b->next;
    }
  }

  /* Finally, look for other possibilities */
  if (!choice)
  {
    if (a && a->parts) 
      b = a->parts;
    else
      b = a;
    while (b)
    {
      if (mutt_can_decode (b))
	choice = b;
      b = b->next;
    }
  }

  if (choice)
  {
    if (s->flags & MUTT_DISPLAY && !option (OPTWEED))
    {
      fseeko (s->fpin, choice->hdr_offset, 0);
      mutt_copy_bytes(s->fpin, s->fpout, choice->offset-choice->hdr_offset);
    }
    mutt_body_handler (choice, s);
  }
  else if (s->flags & MUTT_DISPLAY)
  {
    /* didn't find anything that we could display! */
    state_mark_attach (s);
    state_puts(_("[-- Error:  Could not display any parts of Multipart/Alternative! --]\n"), s);
    rc = -1;
  }

  if (mustfree)
    mutt_free_body(&a);

  return rc;
}

/* handles message/rfc822 body parts */
static int message_handler (BODY *a, STATE *s)
{
  struct stat st;
  BODY *b;
  LOFF_T off_start;
  int rc = 0;

  off_start = ftello (s->fpin);
  if (a->encoding == ENCBASE64 || a->encoding == ENCQUOTEDPRINTABLE || 
      a->encoding == ENCUUENCODED)
  {
    fstat (fileno (s->fpin), &st);
    b = mutt_new_body ();
    b->length = (LOFF_T) st.st_size;
    b->parts = mutt_parse_messageRFC822 (s->fpin, b);
  }
  else
    b = a;

  if (b->parts)
  {
    mutt_copy_hdr (s->fpin, s->fpout, off_start, b->parts->offset,
	(((s->flags & MUTT_WEED) || ((s->flags & (MUTT_DISPLAY|MUTT_PRINTING)) && option (OPTWEED))) ? (CH_WEED | CH_REORDER) : 0) |
	(s->prefix ? CH_PREFIX : 0) | CH_DECODE | CH_FROM |
	((s->flags & MUTT_DISPLAY) ? CH_DISPLAY : 0), s->prefix);

    if (s->prefix)
      state_puts (s->prefix, s);
    state_putc ('\n', s);

    rc = mutt_body_handler (b->parts, s);
  }

  if (a->encoding == ENCBASE64 || a->encoding == ENCQUOTEDPRINTABLE ||
      a->encoding == ENCUUENCODED)
    mutt_free_body (&b);
  
  return rc;
}

/* returns 1 if decoding the attachment will produce output */
int mutt_can_decode (BODY *a)
{
  if (mutt_is_autoview (a))
    return 1;
  else if (a->type == TYPETEXT)
    return (1);
  else if (a->type == TYPEMESSAGE)
    return (1);
  else if (a->type == TYPEMULTIPART)
  {
    BODY *p;

    if (WithCrypto)
    {
      if (ascii_strcasecmp (a->subtype, "signed") == 0 ||
	  ascii_strcasecmp (a->subtype, "encrypted") == 0)
        return (1);
    }

    for (p = a->parts; p; p = p->next)
    {
      if (mutt_can_decode (p))
        return (1);
    }
    
  }
  else if (WithCrypto && a->type == TYPEAPPLICATION)
  {
    if ((WithCrypto & APPLICATION_PGP) && mutt_is_application_pgp(a))
      return (1);
    if ((WithCrypto & APPLICATION_SMIME) && mutt_is_application_smime(a))
      return (1);
  }

  return (0);
}

static int multipart_handler (BODY *a, STATE *s)
{
  BODY *b, *p;
  char length[5];
  struct stat st;
  int count;
  int rc = 0;

  if (a->encoding == ENCBASE64 || a->encoding == ENCQUOTEDPRINTABLE ||
      a->encoding == ENCUUENCODED)
  {
    fstat (fileno (s->fpin), &st);
    b = mutt_new_body ();
    b->length = (long) st.st_size;
    b->parts = mutt_parse_multipart (s->fpin,
		  mutt_get_parameter ("boundary", a->parameter),
		  (long) st.st_size, ascii_strcasecmp ("digest", a->subtype) == 0);
  }
  else
    b = a;

  for (p = b->parts, count = 1; p; p = p->next, count++)
  {
    if (s->flags & MUTT_DISPLAY)
    {
      state_mark_attach (s);
      state_printf (s, _("[-- Attachment #%d"), count);
      if (p->description || p->filename || p->form_name)
      {
	state_puts (": ", s);
	state_puts (p->description ? p->description :
		    p->filename ? p->filename : p->form_name, s);
      }
      state_puts (" --]\n", s);

      mutt_pretty_size (length, sizeof (length), p->length);
      
      state_mark_attach (s);
      state_printf (s, _("[-- Type: %s/%s, Encoding: %s, Size: %s --]\n"),
		    TYPE (p), p->subtype, ENCODING (p->encoding), length);
      if (!option (OPTWEED))
      {
	fseeko (s->fpin, p->hdr_offset, 0);
	mutt_copy_bytes(s->fpin, s->fpout, p->offset-p->hdr_offset);
      }
      else
	state_putc ('\n', s);
    }

    rc = mutt_body_handler (p, s);
    state_putc ('\n', s);
    
    if (rc)
    {
      mutt_error (_("One or more parts of this message could not be displayed"));
      dprint (1, (debugfile, "Failed on attachment #%d, type %s/%s.\n", count, TYPE(p), NONULL (p->subtype)));
    }
    
    if ((s->flags & MUTT_REPLYING)
        && (option (OPTINCLUDEONLYFIRST)) && (s->flags & MUTT_FIRSTDONE))
      break;
  }

  if (a->encoding == ENCBASE64 || a->encoding == ENCQUOTEDPRINTABLE ||
      a->encoding == ENCUUENCODED)
    mutt_free_body (&b);

  /* make failure of a single part non-fatal */
  if (rc < 0)
    rc = 1;
  return rc;
}

static int autoview_handler (BODY *a, STATE *s)
{
  rfc1524_entry *entry = rfc1524_new_entry ();
  char buffer[LONG_STRING];
  char type[STRING];
  char command[LONG_STRING];
  char tempfile[_POSIX_PATH_MAX] = "";
  char *fname;
  FILE *fpin = NULL;
  FILE *fpout = NULL;
  FILE *fperr = NULL;
  int piped = FALSE;
  pid_t thepid;
  int rc = 0;

  snprintf (type, sizeof (type), "%s/%s", TYPE (a), a->subtype);
  rfc1524_mailcap_lookup (a, type, entry, MUTT_AUTOVIEW);

  fname = safe_strdup (a->filename);
  mutt_sanitize_filename (fname, 1);
  rfc1524_expand_filename (entry->nametemplate, fname, tempfile, sizeof (tempfile));
  FREE (&fname);

  if (entry->command)
  {
    strfcpy (command, entry->command, sizeof (command));

    /* rfc1524_expand_command returns 0 if the file is required */
    piped = rfc1524_expand_command (a, tempfile, type, command, sizeof (command));

    if (s->flags & MUTT_DISPLAY)
    {
      state_mark_attach (s);
      state_printf (s, _("[-- Autoview using %s --]\n"), command);
      mutt_message(_("Invoking autoview command: %s"),command);
    }

    if ((fpin = safe_fopen (tempfile, "w+")) == NULL)
    {
      mutt_perror ("fopen");
      rfc1524_free_entry (&entry);
      return -1;
    }
    
    mutt_copy_bytes (s->fpin, fpin, a->length);

    if(!piped)
    {
      safe_fclose (&fpin);
      thepid = mutt_create_filter (command, NULL, &fpout, &fperr);
    }
    else
    {
      unlink (tempfile);
      fflush (fpin);
      rewind (fpin);
      thepid = mutt_create_filter_fd (command, NULL, &fpout, &fperr,
				      fileno(fpin), -1, -1);
    }

    if (thepid < 0)
    {
      mutt_perror _("Can't create filter");
      if (s->flags & MUTT_DISPLAY)
      {
	state_mark_attach (s);
	state_printf (s, _("[-- Can't run %s. --]\n"), command);
      }
      rc = -1;
      goto bail;
    }
    
    if (s->prefix)
    {
      while (fgets (buffer, sizeof(buffer), fpout) != NULL)
      {
        state_puts (s->prefix, s);
        state_puts (buffer, s);
      }
      /* check for data on stderr */
      if (fgets (buffer, sizeof(buffer), fperr)) 
      {
	if (s->flags & MUTT_DISPLAY)
	{
	  state_mark_attach (s);
	  state_printf (s, _("[-- Autoview stderr of %s --]\n"), command);
	}

	state_puts (s->prefix, s);
	state_puts (buffer, s);
	while (fgets (buffer, sizeof(buffer), fperr) != NULL)
	{
	  state_puts (s->prefix, s);
	  state_puts (buffer, s);
	}
      }
    }
    else
    {
      mutt_copy_stream (fpout, s->fpout);
      /* Check for stderr messages */
      if (fgets (buffer, sizeof(buffer), fperr))
      {
	if (s->flags & MUTT_DISPLAY)
	{
	  state_mark_attach (s);
	  state_printf (s, _("[-- Autoview stderr of %s --]\n"), 
			command);
	}
	
	state_puts (buffer, s);
	mutt_copy_stream (fperr, s->fpout);
      }
    }

  bail:
    safe_fclose (&fpout);
    safe_fclose (&fperr);

    mutt_wait_filter (thepid);
    if (piped)
      safe_fclose (&fpin);
    else
      mutt_unlink (tempfile);

    if (s->flags & MUTT_DISPLAY) 
      mutt_clear_error ();
  }
  rfc1524_free_entry (&entry);

  return rc;
}

static int external_body_handler (BODY *b, STATE *s)
{
  const char *access_type;
  const char *expiration;
  time_t expire;

  access_type = mutt_get_parameter ("access-type", b->parameter);
  if (!access_type)
  {
    if (s->flags & MUTT_DISPLAY)
    {
      state_mark_attach (s);
      state_puts (_("[-- Error: message/external-body has no access-type parameter --]\n"), s);
      return 0;
    }
    else
      return -1;
  }

  expiration = mutt_get_parameter ("expiration", b->parameter);
  if (expiration)
    expire = mutt_parse_date (expiration, NULL);
  else
    expire = -1;

  if (!ascii_strcasecmp (access_type, "x-mutt-deleted"))
  {
    if (s->flags & (MUTT_DISPLAY|MUTT_PRINTING))
    {
      char *length;
      char pretty_size[10];
      
      state_mark_attach (s);
      state_printf (s, _("[-- This %s/%s attachment "),
	       TYPE(b->parts), b->parts->subtype);
      length = mutt_get_parameter ("length", b->parameter);
      if (length)
      {
	mutt_pretty_size (pretty_size, sizeof (pretty_size),
			  strtol (length, NULL, 10));
	state_printf (s, _("(size %s bytes) "), pretty_size);
      }
      state_puts (_("has been deleted --]\n"), s);

      if (expire != -1)
      {
	state_mark_attach (s);
	state_printf (s, _("[-- on %s --]\n"), expiration);
      }
      if (b->parts->filename)
      {
	state_mark_attach (s);
	state_printf (s, _("[-- name: %s --]\n"), b->parts->filename);
      }

      mutt_copy_hdr (s->fpin, s->fpout, ftello (s->fpin), b->parts->offset,
		     (option (OPTWEED) ? (CH_WEED | CH_REORDER) : 0) |
		     CH_DECODE , NULL);
    }
  }
  else if(expiration && expire < time(NULL))
  {
    if (s->flags & MUTT_DISPLAY)
    {
      state_mark_attach (s);
      state_printf (s, _("[-- This %s/%s attachment is not included, --]\n"),
		    TYPE(b->parts), b->parts->subtype);
      state_attach_puts (_("[-- and the indicated external source has --]\n"
			   "[-- expired. --]\n"), s);

      mutt_copy_hdr(s->fpin, s->fpout, ftello (s->fpin), b->parts->offset,
		    (option (OPTWEED) ? (CH_WEED | CH_REORDER) : 0) |
		    CH_DECODE | CH_DISPLAY, NULL);
    }
  }
  else
  {
    if (s->flags & MUTT_DISPLAY)
    {
      state_mark_attach (s);
      state_printf (s,
		    _("[-- This %s/%s attachment is not included, --]\n"),
		    TYPE (b->parts), b->parts->subtype);
      state_mark_attach (s);
      state_printf (s, 
		    _("[-- and the indicated access-type %s is unsupported --]\n"),
		    access_type);
      mutt_copy_hdr (s->fpin, s->fpout, ftello (s->fpin), b->parts->offset,
		     (option (OPTWEED) ? (CH_WEED | CH_REORDER) : 0) |
		     CH_DECODE | CH_DISPLAY, NULL);
    }
  }
  
  return 0;
}

void mutt_decode_attachment (BODY *b, STATE *s)
{
  int istext = mutt_is_text_part (b);
  iconv_t cd = (iconv_t)(-1);

  if (istext && s->flags & MUTT_CHARCONV)
  {
    char *charset = mutt_get_parameter ("charset", b->parameter);
    if (!charset && AssumedCharset && *AssumedCharset)
      charset = mutt_get_default_charset ();
    if (charset && Charset)
      cd = mutt_iconv_open (Charset, charset, MUTT_ICONV_HOOK_FROM);
  }
  else if (istext && b->charset)
    cd = mutt_iconv_open (Charset, b->charset, MUTT_ICONV_HOOK_FROM);

  fseeko (s->fpin, b->offset, 0);
  switch (b->encoding)
  {
    case ENCQUOTEDPRINTABLE:
      mutt_decode_quoted (s, b->length, istext || ((WithCrypto & APPLICATION_PGP) && mutt_is_application_pgp (b)), cd);
      break;
    case ENCBASE64:
      mutt_decode_base64 (s, b->length, istext || ((WithCrypto & APPLICATION_PGP) && mutt_is_application_pgp (b)), cd);
      break;
    case ENCUUENCODED:
      mutt_decode_uuencoded (s, b->length, istext || ((WithCrypto & APPLICATION_PGP) && mutt_is_application_pgp (b)), cd);
      break;
    default:
      mutt_decode_xbit (s, b->length, istext || ((WithCrypto & APPLICATION_PGP) && mutt_is_application_pgp (b)), cd);
      break;
  }

  if (cd != (iconv_t)(-1))
    iconv_close (cd);
}

/* when generating format=flowed ($text_flowed is set) from format=fixed,
 * strip all trailing spaces to improve interoperability;
 * if $text_flowed is unset, simply verbatim copy input
 */
static int text_plain_handler (BODY *b, STATE *s)
{
  char *buf = NULL;
  size_t l = 0, sz = 0;

  while ((buf = mutt_read_line (buf, &sz, s->fpin, NULL, 0)))
  {
    if (mutt_strcmp (buf, "-- ") != 0 && option (OPTTEXTFLOWED))
    {
      l = mutt_strlen (buf);
      while (l > 0 && buf[l-1] == ' ')
	buf[--l] = 0;
    }
    if (s->prefix)
      state_puts (s->prefix, s);
    state_puts (buf, s);
    state_putc ('\n', s);
  }

  FREE (&buf);
  return 0;
}

static int run_decode_and_handler (BODY *b, STATE *s, handler_t handler, int plaintext)
{
  int origType;
  char *savePrefix = NULL;
  FILE *fp = NULL;
  char tempfile[_POSIX_PATH_MAX];
  size_t tmplength = 0;
  LOFF_T tmpoffset = 0;
  int decode = 0;
  int rc = 0;

  fseeko (s->fpin, b->offset, 0);

  /* see if we need to decode this part before processing it */
  if (b->encoding == ENCBASE64 || b->encoding == ENCQUOTEDPRINTABLE ||
      b->encoding == ENCUUENCODED || plaintext ||
      mutt_is_text_part (b))				/* text subtypes may
                                                        * require character
                                                        * set conversion even
                                                        * with 8bit encoding.
                                                        */
  {
    origType = b->type;

    if (!plaintext)
    {
      /* decode to a tempfile, saving the original destination */
      fp = s->fpout;
      mutt_mktemp (tempfile, sizeof (tempfile));
      if ((s->fpout = safe_fopen (tempfile, "w")) == NULL)
      {
        mutt_error _("Unable to open temporary file!");
        dprint (1, (debugfile, "Can't open %s.\n", tempfile));
        return -1;
      }
      /* decoding the attachment changes the size and offset, so save a copy
        * of the "real" values now, and restore them after processing
        */
      tmplength = b->length;
      tmpoffset = b->offset;

      /* if we are decoding binary bodies, we don't want to prefix each
        * line with the prefix or else the data will get corrupted.
        */
      savePrefix = s->prefix;
      s->prefix = NULL;

      decode = 1;
    }
    else
      b->type = TYPETEXT;

    mutt_decode_attachment (b, s);

    if (decode)
    {
      b->length = ftello (s->fpout);
      b->offset = 0;
      safe_fclose (&s->fpout);

      /* restore final destination and substitute the tempfile for input */
      s->fpout = fp;
      fp = s->fpin;
      s->fpin = fopen (tempfile, "r");
      unlink (tempfile);

      /* restore the prefix */
      s->prefix = savePrefix;
    }

    b->type = origType;
  }

  /* process the (decoded) body part */
  if (handler)
  {
    rc = handler (b, s);

    if (rc)
    {
      dprint (1, (debugfile, "Failed on attachment of type %s/%s.\n", TYPE(b), NONULL (b->subtype)));
    }

    if (decode)
    {
      b->length = tmplength;
      b->offset = tmpoffset;

      /* restore the original source stream */
      safe_fclose (&s->fpin);
      s->fpin = fp;
    }
  }
  s->flags |= MUTT_FIRSTDONE;

  return rc;
}

static int valid_pgp_encrypted_handler (BODY *b, STATE *s)
{
  int rc;
  BODY *octetstream;

  octetstream = b->parts->next;
  rc = crypt_pgp_encrypted_handler (octetstream, s);
  b->goodsig |= octetstream->goodsig;

  return rc;
}

static int malformed_pgp_encrypted_handler (BODY *b, STATE *s)
{
  int rc;
  BODY *octetstream;

  octetstream = b->parts->next->next;
  /* exchange encodes the octet-stream, so re-run it through the decoder */
  rc = run_decode_and_handler (octetstream, s, crypt_pgp_encrypted_handler, 0);
  b->goodsig |= octetstream->goodsig;

  return rc;
}

int mutt_body_handler (BODY *b, STATE *s)
{
  int plaintext = 0;
  handler_t handler = NULL;
  int rc = 0;

  int oflags = s->flags;
  
  /* first determine which handler to use to process this part */

  if (mutt_is_autoview (b))
  {
    handler = autoview_handler;
    s->flags &= ~MUTT_CHARCONV;
  }
  else if (b->type == TYPETEXT)
  {
    if (ascii_strcasecmp ("plain", b->subtype) == 0)
    {
      /* avoid copying this part twice since removing the transfer-encoding is
       * the only operation needed.
       */
      if ((WithCrypto & APPLICATION_PGP) && mutt_is_application_pgp (b))
	handler = crypt_pgp_application_pgp_handler;
      else if (option(OPTREFLOWTEXT) && ascii_strcasecmp ("flowed", mutt_get_parameter ("format", b->parameter)) == 0)
	handler = rfc3676_handler;
      else
	handler = text_plain_handler;
    }
    else if (ascii_strcasecmp ("enriched", b->subtype) == 0)
      handler = text_enriched_handler;
    else /* text body type without a handler */
      plaintext = 1;
  }
  else if (b->type == TYPEMESSAGE)
  {
    if(mutt_is_message_type(b->type, b->subtype))
      handler = message_handler;
    else if (!ascii_strcasecmp ("delivery-status", b->subtype))
      plaintext = 1;
    else if (!ascii_strcasecmp ("external-body", b->subtype))
      handler = external_body_handler;
  }
  else if (b->type == TYPEMULTIPART)
  {
    char *p;

    if (ascii_strcasecmp ("alternative", b->subtype) == 0)
      handler = alternative_handler;
    else if (WithCrypto && ascii_strcasecmp ("signed", b->subtype) == 0)
    {
      p = mutt_get_parameter ("protocol", b->parameter);

      if (!p)
        mutt_error _("Error: multipart/signed has no protocol.");
      else if (s->flags & MUTT_VERIFY)
	handler = mutt_signed_handler;
    }
    else if (mutt_is_valid_multipart_pgp_encrypted (b))
      handler = valid_pgp_encrypted_handler;
    else if (mutt_is_malformed_multipart_pgp_encrypted (b))
      handler = malformed_pgp_encrypted_handler;

    if (!handler)
      handler = multipart_handler;

    if (b->encoding != ENC7BIT && b->encoding != ENC8BIT
        && b->encoding != ENCBINARY)
    {
      dprint (1, (debugfile, "Bad encoding type %d for multipart entity, "
                  "assuming 7 bit\n", b->encoding));
      b->encoding = ENC7BIT;
    }
  }
  else if (WithCrypto && b->type == TYPEAPPLICATION)
  {
    if (option (OPTDONTHANDLEPGPKEYS)
        && !ascii_strcasecmp("pgp-keys", b->subtype))
    {
      /* pass raw part through for key extraction */
      plaintext = 1;
    }
    else if ((WithCrypto & APPLICATION_PGP) && mutt_is_application_pgp (b))
      handler = crypt_pgp_application_pgp_handler;
    else if ((WithCrypto & APPLICATION_SMIME) && mutt_is_application_smime(b))
      handler = crypt_smime_application_smime_handler;
  }

  /* only respect disposition == attachment if we're not
     displaying from the attachment menu (i.e. pager) */
  if ((!option (OPTHONORDISP) || (b->disposition != DISPATTACH ||
				  option(OPTVIEWATTACH))) &&
       (plaintext || handler))
  {
    rc = run_decode_and_handler (b, s, handler, plaintext);
  }
  /* print hint to use attachment menu for disposition == attachment
     if we're not already being called from there */
  else if ((s->flags & MUTT_DISPLAY) || (b->disposition == DISPATTACH &&
				      !option (OPTVIEWATTACH) &&
				      option (OPTHONORDISP) &&
				      (plaintext || handler)))
  {
    state_mark_attach (s);
    if (option (OPTHONORDISP) && b->disposition == DISPATTACH)
      fputs (_("[-- This is an attachment "), s->fpout);
    else
      state_printf (s, _("[-- %s/%s is unsupported "), TYPE (b), b->subtype);
    if (!option (OPTVIEWATTACH))
    {
      char keystroke[SHORT_STRING];

      if (km_expand_key (keystroke, sizeof(keystroke),
			km_find_func (MENU_PAGER, OP_VIEW_ATTACHMENTS)))
	fprintf (s->fpout, _("(use '%s' to view this part)"), keystroke);
      else
	fputs (_("(need 'view-attachments' bound to key!)"), s->fpout);
    }
    fputs (" --]\n", s->fpout);
  }

  s->flags = oflags | (s->flags & MUTT_FIRSTDONE);
  if (rc)
  {
    dprint (1, (debugfile, "Bailing on attachment of type %s/%s.\n", TYPE(b), NONULL (b->subtype)));
  }

  return rc;
}
