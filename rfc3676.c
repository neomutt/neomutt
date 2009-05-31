/*
 * Copyright (C) 2005 Andreas Krennmair <ak@synflood.at>
 * Copyright (C) 2005 Peter J. Holzer <hjp@hjp.net>
 * Copyright (C) 2005-9 Rocco Rutte <pdmef@gmx.net>
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

static size_t print_indent (int ql, STATE *s, int sp)
{
  int i;
  size_t len = 0;

  if (s->prefix)
  {
    /* use given prefix only for format=fixed replies to format=flowed,
     * for format=flowed replies to format=flowed, use '>' indentation
     */
    if (option (OPTTEXTFLOWED))
      ql++;
    else
    {
      state_puts (s->prefix, s);
      len = mutt_strlen (s->prefix);
      sp = 0;
    }
  }
  for (i = 0; i < ql; i++)
    state_putc ('>', s);
  if (sp)
    state_putc (' ', s);
  return ql + sp + len;
}

static void flush_par (STATE *s, size_t *sofar)
{
  if (*sofar > 0)
  {
    state_putc ('\n', s);
    *sofar = 0;
  }
}

static int quote_width (STATE *s, int ql)
{
  size_t width = (Wrap ? mutt_term_width (Wrap) : FLOWED_MAX) - 1;
  if (s->flags & M_REPLYING && width > FLOWED_MAX)
    width = FLOWED_MAX;
  if (ql + 1 < width)
    width -= ql + 1;
  return width;
}

static void print_flowed_line (char *line, STATE *s, int ql, size_t *sofar, int term)
{
  size_t width, w, words;
  char *p;

  if (!line || !*line)
  {
    /* flush current paragraph (if any) first */
    flush_par (s, sofar);
    print_indent (ql, s, 0);
    state_putc ('\n', s);
    return;
  }

  width = quote_width (s, ql);

  dprint (4, (debugfile, "f=f: line [%s], width = %ld\n", NONULL(line), (long)width));

  for (p = (char *)line, words = 0; (p = strsep (&line, " ")) != NULL ; )
  {
    w = mutt_strwidth (NONULL(p));
    dprint (4, (debugfile, "f=f: word [%s], width = %ld, line = %ld\n", NONULL(p), (long)w, (long)*sofar));
    if (w + 1 + (*sofar) > width)
    {
      /* line would be too long, flush; for format=flowed we keep a
       * trailing space but remove it otherwise for interoperability
       */
      dprint (4, (debugfile, "f=f: width: %ld\n", (long)*sofar));
      state_puts (option (OPTTEXTFLOWED) ? " \n" : "\n", s);
      *sofar = 0;
    }
    if (*sofar == 0)
    {
      /* indent empty lines */
      *sofar = print_indent (ql, s, ql > 0 || s->prefix);
      words = 0;
    }
    if (words > 0)
    {
      /* put space before current word if we have words already,
         and the current word isn't the trailing space */
      if (w > 0)
	state_putc (' ', s);
      (*sofar)++;
    }
    state_puts (NONULL(p), s);
    (*sofar) += w;
    words++;
  }

  if (term)
    flush_par (s, sofar);
}

static void print_fixed_line (const char *line, STATE *s, int ql, size_t *sofar)
{
  int len = mutt_strlen (line);

  if (len == 0)
  {
    print_indent (ql, s, 0);
    state_putc ('\n', s);
    return;
  }

  print_indent (ql, s, ql > 0 || s->prefix);
  state_puts (line, s);
  state_putc ('\n', s);

  *sofar = 0;
}

int rfc3676_handler (BODY * a, STATE * s)
{
  int bytes = a->length;
  char buf[LONG_STRING];
  char *t = NULL;
  unsigned int quotelevel = 0, newql = 0, sigsep = 0;
  int buf_off = 0, buf_len;
  int delsp = 0, fixed = 0;
  size_t width = 0;

  /* respect DelSp of RfC3676 only with f=f parts */
  if ((t = (char *) mutt_get_parameter ("delsp", a->parameter)))
  {
    delsp = mutt_strlen (t) == 3 && ascii_strncasecmp (t, "yes", 3) == 0;
    t = NULL;
  }

  dprint (4, (debugfile, "f=f: DelSp: %s\n", delsp ? "yes" : "no"));

  while (bytes > 0 && fgets (buf, sizeof (buf), s->fpin))
  {

    buf_len = mutt_strlen (buf);
    bytes -= buf_len;

    newql = get_quote_level (buf);

    /* end flowed paragraph (if we're within one) if quoting level
     * changes (should not but can happen, see RFC 3676, sec. 4.5.)
     */
    if (newql != quotelevel)
      flush_par (s, &width);

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

    /* print fixed-and-standalone, fixed-and-empty and sigsep lines as
     * fixed lines */
    if ((fixed && (!width || !buf_len)) || sigsep)
    {
      /* if we're within a flowed paragraph, terminate it */
      flush_par (s, &width);
      print_fixed_line (buf + buf_off, s, quotelevel, &width);
      continue;
    }

    /* for DelSp=yes, we need to strip one SP prior to CRLF on flowed lines */
    if (delsp && !fixed)
      buf[--buf_len] = '\0';

    print_flowed_line (buf + buf_off, s, quotelevel, &width, fixed);
  }

  flush_par (s, &width);

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
    safe_fclose (&in);
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
  safe_fclose (&in);
  safe_fclose (&out);
  mutt_set_mtime (hdr->content->filename, tmpfile);
  unlink (hdr->content->filename);
  mutt_str_replace (&hdr->content->filename, tmpfile);
}
