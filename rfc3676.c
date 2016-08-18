/*
 * Copyright (C) 2005 Andreas Krennmair <ak@synflood.at>
 * Copyright (C) 2005 Peter J. Holzer <hjp@hjp.net>
 * Copyright (C) 2005-2009 Rocco Rutte <pdmef@gmx.net>
 * Copyright (C) 2010 Michael R. Elkins <me@mutt.org>
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

#define FLOWED_MAX 72

typedef struct flowed_state
{
  size_t width;
  size_t spaces;
  int delsp;
} flowed_state_t;

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

/* Determines whether to add spacing between/after each quote level:
 *    >>>foo
 * becomes
 *    > > > foo
 */
static int space_quotes (STATE *s)
{
  /* Allow quote spacing in the pager even for OPTTEXTFLOWED,
   * but obviously not when replying.
   */
  if (option (OPTTEXTFLOWED) && (s->flags & MUTT_REPLYING))
    return 0;

  return option (OPTREFLOWSPACEQUOTES);
}

/* Determines whether to add a trailing space to quotes:
 *    >>> foo
 * as opposed to
 *    >>>foo
 */
static int add_quote_suffix (STATE *s, int ql)
{
  if (s->flags & MUTT_REPLYING)
    return 0;

  if (space_quotes (s))
    return 0;

  if (!ql && !s->prefix)
    return 0;

  /* The prefix will add its own space */
  if (!option (OPTTEXTFLOWED) && !ql && s->prefix)
    return 0;

  return 1;
}

static size_t print_indent (int ql, STATE *s, int add_suffix)
{
  int i;
  size_t wid = 0;

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
      wid = mutt_strwidth (s->prefix);
    }
  }
  for (i = 0; i < ql; i++)
  {
    state_putc ('>', s);
    if (space_quotes (s) )
      state_putc (' ', s);
  }
  if (add_suffix)
    state_putc (' ', s);

  if (space_quotes (s))
    ql *= 2;

  return ql + add_suffix + wid;
}

static void flush_par (STATE *s, flowed_state_t *fst)
{
  if (fst->width > 0)
  {
    state_putc ('\n', s);
    fst->width = 0;
  }
  fst->spaces = 0;
}

/* Calculate the paragraph width based upon the current quote level. The start
 * of a quoted line will be ">>> ", so we need to subtract the space required
 * for the prefix from the terminal width. */
static int quote_width (STATE *s, int ql)
{
  int width = mutt_window_wrap_cols (MuttIndexWindow, ReflowWrap);
  if (option(OPTTEXTFLOWED) && (s->flags & MUTT_REPLYING))
  {
    /* When replying, force a wrap at FLOWED_MAX to comply with RFC3676
     * guidelines */
    if (width > FLOWED_MAX)
      width = FLOWED_MAX;
    ++ql; /* When replying, we will add an additional quote level */
  }
  /* adjust the paragraph width subtracting the number of prefix chars */
  width -= space_quotes (s) ? ql*2 : ql;
  /* When displaying (not replying), there may be a space between the prefix
   * string and the paragraph */
  if (add_quote_suffix (s, ql))
    --width;
  /* failsafe for really long quotes */
  if (width <= 0)
    width = FLOWED_MAX; /* arbitrary, since the line will wrap */
  return width;
}

static void print_flowed_line (char *line, STATE *s, int ql,
			       flowed_state_t *fst, int term)
{
  size_t width, w, words = 0;
  char *p;
  char last;

  if (!line || !*line)
  {
    /* flush current paragraph (if any) first */
    flush_par (s, fst);
    print_indent (ql, s, 0);
    state_putc ('\n', s);
    return;
  }

  width = quote_width (s, ql);
  last = line[mutt_strlen (line) - 1];

  dprint (4, (debugfile, "f=f: line [%s], width = %ld, spaces = %d\n",
	      NONULL(line), (long)width, fst->spaces));

  for (p = (char *)line, words = 0; (p = strsep (&line, " ")) != NULL ; )
  {
    dprint(4,(debugfile,"f=f: word [%s], width: %d, remaining = [%s]\n",
	      p, fst->width, line));

    /* remember number of spaces */
    if (!*p)
    {
      dprint(4,(debugfile,"f=f: additional space\n"));
      fst->spaces++;
      continue;
    }
    /* there's exactly one space prior to every but the first word */
    if (words)
      fst->spaces++;

    w = mutt_strwidth (p);
    /* see if we need to break the line but make sure the first
       word is put on the line regardless;
       if for DelSp=yes only one trailing space is used, we probably
       have a long word that we should break within (we leave that
       up to the pager or user) */
    if (!(!fst->spaces && fst->delsp && last != ' ') &&
	w < width && w + fst->width + fst->spaces > width)
    {
      dprint(4,(debugfile,"f=f: break line at %d, %d spaces left\n",
		fst->width, fst->spaces));
      /* only honor trailing spaces for format=flowed replies */
      if (option(OPTTEXTFLOWED))
	for ( ; fst->spaces; fst->spaces--)
	  state_putc (' ', s);
      state_putc ('\n', s);
      fst->width = 0;
      fst->spaces = 0;
      words = 0;
    }

    if (!words && !fst->width)
      fst->width = print_indent (ql, s, add_quote_suffix (s, ql));
    fst->width += w + fst->spaces;
    for ( ; fst->spaces; fst->spaces--)
      state_putc (' ', s);
    state_puts (p, s);
    words++;
  }

  if (term)
    flush_par (s, fst);
}

static void print_fixed_line (const char *line, STATE *s, int ql,
			      flowed_state_t *fst)
{
  print_indent (ql, s, add_quote_suffix (s, ql));
  if (line && *line)
    state_puts (line, s);
  state_putc ('\n', s);

  fst->width = 0;
  fst->spaces = 0;
}

int rfc3676_handler (BODY * a, STATE * s)
{
  char *buf = NULL, *t = NULL;
  unsigned int quotelevel = 0, newql = 0, sigsep = 0;
  int buf_off = 0, delsp = 0, fixed = 0;
  size_t buf_len = 0, sz = 0;
  flowed_state_t fst;

  memset (&fst, 0, sizeof (fst));

  /* respect DelSp of RfC3676 only with f=f parts */
  if ((t = (char *) mutt_get_parameter ("delsp", a->parameter)))
  {
    delsp = mutt_strlen (t) == 3 && ascii_strncasecmp (t, "yes", 3) == 0;
    t = NULL;
    fst.delsp = 1;
  }

  dprint (4, (debugfile, "f=f: DelSp: %s\n", delsp ? "yes" : "no"));

  while ((buf = mutt_read_line (buf, &sz, s->fpin, NULL, 0)))
  {
    buf_len = mutt_strlen (buf);
    newql = get_quote_level (buf);

    /* end flowed paragraph (if we're within one) if quoting level
     * changes (should not but can happen, see RFC 3676, sec. 4.5.)
     */
    if (newql != quotelevel)
      flush_par (s, &fst);

    quotelevel = newql;
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
    if ((fixed && (!fst.width || !buf_len)) || sigsep)
    {
      /* if we're within a flowed paragraph, terminate it */
      flush_par (s, &fst);
      print_fixed_line (buf + buf_off, s, quotelevel, &fst);
      continue;
    }

    /* for DelSp=yes, we need to strip one SP prior to CRLF on flowed lines */
    if (delsp && !fixed)
      buf[--buf_len] = '\0';

    print_flowed_line (buf + buf_off, s, quotelevel, &fst, fixed);
  }

  flush_par (s, &fst);

  FREE (&buf);
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

  mutt_mktemp (tmpfile, sizeof (tmpfile));
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
