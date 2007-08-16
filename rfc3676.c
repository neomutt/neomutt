/*
 * Copyright (C) 2005 Andreas Krennmair <ak@synflood.at>
 * Copyright (C) 2005 Peter J. Holzer <hjp@hjp.net>
 * Copyright (C) 2005-7 Rocco Rutte <pdmef@gmx.net>
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
 *
 */ 

/* This file was originally part of mutt-ng */

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
#include "ascii.h"
#include "lib.h"

#define FLOWED_MAX 77

static int get_quote_level (const char *line)
{
  int quoted = 0;
  char *p = (char *) line;

  while (p && *p == '>')
  {
    quoted++;
    p++;
  }

  return quoted;
}

static void print_indent (int ql, STATE *s)
{
  int i;

  if (s->prefix)
    ql++;
  for (i = 0; i < ql; i++)
    state_putc ('>', s);
}

static void print_flowed_line (const char *line, STATE *s, int ql)
{
  int width;
  char *pos, *oldpos;
  int len = mutt_strlen (line);

  width = (Wrap ? mutt_term_width (Wrap) : FLOWED_MAX) - 1;
  if (s->flags & M_REPLYING && width > FLOWED_MAX)
    width = FLOWED_MAX;
  /* probably want a quote_width function */
  if (ql + 1 < width)
    width -= ql + 1;
    
  if (len == 0)
  {
    print_indent (ql, s);
    state_putc ('\n', s);
    return;
  }

  pos = (char *) line + width;
  oldpos = (char *) line;

  for (; oldpos < line + len; pos += width)
  {
    /* only search for a new position when we're not over the end */
    if (pos < line + len)
    {
      if (*pos == ' ')
      {
        dprint (4, (debugfile, "f=f: found space directly at width\n"));
        *pos = '\0';
        ++pos;
      }
      else
      {
        char *save = pos;
        dprint (4, (debugfile, "f=f: need to search for space\n"));

        while (pos >= oldpos && *pos != ' ')
          --pos;

        if (pos < oldpos)
        {
          dprint (4, (debugfile, "f=f: no space found while searching "
                           "to left; going right\n"));
          pos = save;
          while (pos < line + len && *pos && *pos != ' ')
            ++pos;
          dprint (4, (debugfile, "f=f: found space at pos %d\n", pos-line));
        }
        else
        {
          dprint (4, (debugfile, "f=f: found space while searching to left\n"));
        }

        *pos = '\0';
        ++pos;
      }
    }
    else
    {
      dprint (4, (debugfile, "f=f: line completely fits on screen\n"));
    }

    print_indent (ql, s);
    if (ql > 0 || s->prefix)
      state_putc (' ', s);
    state_puts (oldpos, s);

    if (pos < line + len)
      state_putc (' ', s);
    state_putc ('\n', s);
    oldpos = pos;
  }
}

int rfc3676_handler (BODY * a, STATE * s)
{
  int bytes = a->length;
  char buf[LONG_STRING];
  char *curline = safe_malloc (STRING);
  char *t = NULL;
  unsigned int curline_len = 1, quotelevel = 0, newql = 0, sigsep = 0;
  int buf_off, buf_len;
  int delsp = 0, fixed = 0;

  *curline = '\0';

  /* respect DelSp of RfC3676 only with f=f parts */
  if ((t = (char *) mutt_get_parameter ("delsp", a->parameter)))
  {
    delsp = mutt_strlen (t) == 3 && ascii_strncasecmp (t, "yes", 3) == 0;
    t = NULL;
  }

  dprint (2, (debugfile, "f=f: DelSp: %s\n", delsp ? "yes" : "no"));

  while (bytes > 0 && fgets (buf, sizeof (buf), s->fpin))
  {

    buf_len = mutt_strlen (buf);
    bytes -= buf_len;

    newql = get_quote_level (buf);

    /* a change of quoting level in a paragraph - shouldn't happen, 
     * but has to be handled - see RFC 3676, sec. 4.5.
     */
    if (newql != quotelevel && *curline)
    {
      print_flowed_line (curline, s, quotelevel);
      *curline = '\0';
      curline_len = 1;
    }
    quotelevel = newql;

    /* XXX - If a line is longer than buf (shouldn't happen), it is split.
     * This will almost always cause an unintended line break, and 
     * possibly a change in quoting level. But that's better than not
     * displaying it at all.
     */
    if ((t = strrchr (buf, '\r')) || (t = strrchr (buf, '\n')))
    {
      *t = '\0';
      buf_len = t - buf;
    }

    buf_off = newql;

    /* respect sender's space-stuffing by removing one leading space */
    if (buf[buf_off] == ' ')
      buf_off++;

    /* test for signature separator */
    sigsep = ascii_strcmp (buf + buf_off, "-- ") == 0;

    /* a fixed line either has no trailing space or is the
     * signature separator */
    fixed = buf_len == buf_off || buf[buf_len - 1] != ' ' || sigsep;

    /* for DelSp=yes, we need to strip one SP prior to CRLF on flowed lines */
    if (delsp && !fixed)
      buf[--buf_len] = '\0';

    /* signature separator also flushes the previous paragraph */
    if (sigsep && *curline)
    {
      print_flowed_line (curline, s, quotelevel);
      *curline = '\0';
      curline_len = 1;
    }

    /* append remaining contents without quotes, space-stuffed
     * spaces and with 1 trailing space (0 or 1 for DelSp=yes) */
    safe_realloc (&curline, curline_len + buf_len - buf_off);
    strcpy (curline + curline_len - 1, buf + buf_off);		/* __STRCPY_CHECKED__ */
    curline_len += buf_len - buf_off;

    /* if this was a fixed line, the paragraph is finished */
    if (fixed)
    {
      print_flowed_line (curline, s, quotelevel);
      *curline = '\0';
      curline_len = 1;
    }

  }

  if (*curline)
  {
    dprint (2, (debugfile, "f=f: still content buffered af EOF, flushing at ql=%d\n", quotelevel));
    print_flowed_line (curline, s, quotelevel);
  }

  FREE(&curline);
  return (0);
}

/*
 * This routine does RfC3676 space stuffing since it's a MUST.
 * Space stuffing means that we have to add leading spaces to
 * certain lines:
 *   - lines starting with a space
 *   - lines starting with 'From '
 * This routine is only called once right after editing the
 * initial message so it's up to the user to take care of stuffing
 * when editing the message several times before actually sending it
 *
 * This is more or less a hack as it replaces the message's content with
 * a freshly created copy in a tempfile and modifies the file's mtime
 * so we don't trigger code paths watching for mtime changes
 */
void rfc3676_space_stuff (HEADER* hdr)
{
#if DEBUG
  int lc = 0;
  size_t len = 0;
  unsigned char c = '\0';
#endif
  FILE *in = NULL, *out = NULL;
  char buf[LONG_STRING];
  char tmpfile[_POSIX_PATH_MAX];

  if (!hdr || !hdr->content || !hdr->content->filename)
    return;

  dprint (2, (debugfile, "f=f: postprocess %s\n", hdr->content->filename));

  if ((in = safe_fopen (hdr->content->filename, "r")) == NULL)
    return;

  mutt_mktemp (tmpfile);
  if ((out = safe_fopen (tmpfile, "w+")) == NULL)
  {
    fclose (in);
    return;
  }

  while (fgets (buf, sizeof (buf), in))
  {
    if (ascii_strncmp ("From ", buf, 5) == 0 || buf[0] == ' ') {
      fputc (' ', out);
#if DEBUG
      lc++;
      len = mutt_strlen (buf);
      if (len > 0)
      {
        c = buf[len-1];
        buf[len-1] = '\0';
      }
      dprint (4, (debugfile, "f=f: line %d needs space-stuffing: '%s'\n",
                  lc, buf));
      if (len > 0)
        buf[len-1] = c;
#endif
    }
    fputs (buf, out);
  }
  fclose (in);
  fclose (out);
  mutt_set_mtime (hdr->content->filename, tmpfile);
  unlink (hdr->content->filename);
  mutt_str_replace (&hdr->content->filename, tmpfile);
}
